#include "shared.h"
#include "model.h"
#include "controller.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>

struct shmbuf *global_shmp;

typedef enum
{
    THEME_LIGHT,
    THEME_DARK,
    THEME_PINK
} ThemeType;

typedef struct
{
    GtkWidget *window;
    GtkWidget *entry;
    GtkWidget *text_view;
    GtkWidget *msg_view;
    char shell_name[MAX_NAME_LEN];
    char history[HISTORY_SIZE];
    size_t cnt;
    char last_messages[HISTORY_SIZE];
    gboolean notification_active;
    ThemeType current_theme;
    GtkWidget *theme_button;
    GtkCssProvider *css_provider;
    char command_history[100][256];
    int command_count;
    int current_command_index;
    int notification_blink_count; 
} Terminal;

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    Terminal *term = (Terminal *)data;

    if (event->keyval == GDK_KEY_Up)
    {
        if (term->command_count > 0 && term->current_command_index < term->command_count - 1)
        {
            term->current_command_index++;
            gtk_entry_set_text(GTK_ENTRY(term->entry),
                               term->command_history[term->command_count - 1 - term->current_command_index]);
        }
        return TRUE;
    }
    else if (event->keyval == GDK_KEY_Down)
    {
        if (term->current_command_index >= 0)
        {
            term->current_command_index--;
            if (term->current_command_index == -1)
                gtk_entry_set_text(GTK_ENTRY(term->entry), "");
            else
                gtk_entry_set_text(GTK_ENTRY(term->entry),
                                   term->command_history[term->command_count - 1 - term->current_command_index]);
        }
        return TRUE;
    }

    return FALSE;
}

static void apply_text_color(GtkTextBuffer *buffer, const char *text, GtkTextIter *end, int current_shell)
{
    int shell_num, target_shell;
    if (sscanf(text, "[%d|Terminal %*d ➡️ %d]:", &shell_num, &target_shell) == 2)
    {
        if (target_shell != current_shell)
            return;
    }
    else
    {
        sscanf(text, "[%d|", &shell_num);
    }

    const char *colors[] = {"red", "green", "blue", "purple", "orange"};
    int num_colors = sizeof(colors) / sizeof(colors[0]);
    char tag_name[32];
    snprintf(tag_name, sizeof(tag_name), "color_%d", shell_num);

    if (!gtk_text_tag_table_lookup(gtk_text_buffer_get_tag_table(buffer), tag_name))
    {
        gtk_text_buffer_create_tag(buffer, tag_name, "foreground", colors[(shell_num - 1) % num_colors], NULL);
    }

    gtk_text_buffer_insert_with_tags_by_name(buffer, end, text, -1, tag_name, NULL);
    gtk_text_buffer_insert(buffer, end, "\n", -1);
}

static void execute_command(GtkWidget *widget, gpointer data)
{
    Terminal *term = (Terminal *)data;
    const char *command = gtk_entry_get_text(GTK_ENTRY(term->entry));

    if (strlen(command) > 0)
    {
        if (term->command_count < 100)
        {
            strncpy(term->command_history[term->command_count], command, 255);
            term->command_history[term->command_count][255] = '\0';
            term->command_count++;
        }
        else
        {
            memmove(term->command_history[0], term->command_history[1], 99 * 256);
            strncpy(term->command_history[99], command, 255);
            term->command_history[99][255] = '\0';
        }
        term->current_command_index = -1;
    }

    handle_input(global_shmp, term->history, &term->cnt, command, term->shell_name);
    gtk_entry_set_text(GTK_ENTRY(term->entry), "");

    char messages[BUF_SIZE];
    int current_shell = atoi(term->shell_name + 9);
    model_read_messages(global_shmp, messages, current_shell);

    GtkTextBuffer *history_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(term->text_view));
    gtk_text_buffer_set_text(history_buffer, "", -1);
    GtkTextIter history_end;
    gtk_text_buffer_get_end_iter(history_buffer, &history_end);
    if (term->cnt > 0)
        gtk_text_buffer_insert(history_buffer, &history_end, term->history, -1);

    GtkTextBuffer *msg_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(term->msg_view));
    gtk_text_buffer_set_text(msg_buffer, "", -1);
    GtkTextIter msg_end;
    gtk_text_buffer_get_end_iter(msg_buffer, &msg_end);

    char *line = strtok(messages, "\n");
    while (line != NULL && strlen(line) > 0)
    {
        apply_text_color(msg_buffer, line, &msg_end, current_shell);
        line = strtok(NULL, "\n");
    }
}

static gboolean blink_notification(gpointer data)
{
    Terminal *term = (Terminal *)data;
    GtkStyleContext *context = gtk_widget_get_style_context(term->msg_view);

    if (term->notification_blink_count % 2 == 0)
        gtk_style_context_add_class(context, "blink");
    else
        gtk_style_context_remove_class(context, "blink");

    term->notification_blink_count--;
    if (term->notification_blink_count <= 0)
    {
        gtk_style_context_remove_class(context, "blink");
        return FALSE;
    }
    return TRUE;
}

static gboolean update_display(gpointer data)
{
    Terminal *term = (Terminal *)data;
    char messages[BUF_SIZE];
    int current_shell = atoi(term->shell_name + 9);
    model_read_messages(global_shmp, messages, current_shell);

    if (strcmp(messages, term->last_messages) != 0)
    {
        if (!term->notification_active)
        {
            term->notification_active = TRUE;
            term->notification_blink_count = 6;
            g_timeout_add(200, blink_notification, term);
        }
        strncpy(term->last_messages, messages, HISTORY_SIZE - 1);
    }
    else if (term->notification_active)
    {
        term->notification_active = FALSE;
    }

    GtkTextBuffer *history_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(term->text_view));
    gtk_text_buffer_set_text(history_buffer, "", -1);
    GtkTextIter history_end;
    gtk_text_buffer_get_end_iter(history_buffer, &history_end);
    if (term->cnt > 0)
        gtk_text_buffer_insert(history_buffer, &history_end, term->history, -1);

    GtkTextBuffer *msg_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(term->msg_view));
    gtk_text_buffer_set_text(msg_buffer, "", -1);
    GtkTextIter msg_end;
    gtk_text_buffer_get_end_iter(msg_buffer, &msg_end);

    char *line = strtok(messages, "\n");
    while (line != NULL && strlen(line) > 0)
    {
        apply_text_color(msg_buffer, line, &msg_end, current_shell);
        line = strtok(NULL, "\n");
    }
    return TRUE;
}

static void apply_css(Terminal *term)
{
    const gchar *css;
    switch (term->current_theme)
    {
    case THEME_LIGHT:
        css = "window { background-color: #f5f5f5; border-radius: 8px; transition: background-color 0.3s; }"
              "textview { background-color: #ffffff; color: #333333; font-family: Fira Code, monospace; font-size: 12px; padding: 10px; transition: background-color 0.3s, color 0.3s; }"
              "textview text { background-color: #ffffff; }"
              "entry { background-color: #e8ecef; color: #333333; font-family: Fira Code, monospace; font-size: 12px; padding: 8px; border-radius: 5px; border: 1px solid #ced4da; transition: background-color 0.3s, color 0.3s; }"
              "label { color: #333333; font-family: Sans; font-size: 11px; transition: color 0.3s; }"
              "button { background-color: #007bff; color: #ffffff; border-radius: 5px; padding: 6px 12px; font-size: 11px; border: none; transition: background-color 0.3s; }"
              "button:hover { background-color: #0056b3; }"
              ".blink { border: 2px solid red; }";
        gtk_button_set_label(GTK_BUTTON(term->theme_button), "Karanlık");
        break;
    case THEME_DARK:
        css = "window { background-color: #212529; border-radius: 8px; transition: background-color 0.3s; }"
              "textview { background-color: #343a40; color: #ffffff; font-family: Fira Code, monospace; font-size: 12px; padding: 10px; transition: background-color 0.3s, color 0.3s; }"
              "textview text { background-color: #343a40; color: #ffffff; }"
              "entry { background-color: #495057; color: #ffffff; font-family: Fira Code, monospace; font-size: 12px; padding: 8px; border-radius: 5px; border: 1px solid #6c757d; transition: background-color 0.3s, color 0.3s; }"
              "label { color: #e83e8c; font-family: Sans; font-size: 11px; transition: color 0.3s; }"
              "button { background-color: #17a2b8; color: #ffffff; border-radius: 5px; padding: 6px 12px; font-size: 11px; border: none; transition: background-color 0.3s; }"
              "button:hover { background-color: #117a8b; }"
              ".blink { border: 2px solid red; }";
        gtk_button_set_label(GTK_BUTTON(term->theme_button), "Pembe");
        break;
    case THEME_PINK:
        css = "window { background-color: #fce4ec; border-radius: 8px; transition: background-color 0.3s; }"
              "textview { background-color: #fff5f7; color: #4a4a4a; font-family: Fira Code, monospace; font-size: 12px; padding: 10px; transition: background-color 0.3s, color 0.3s; }"
              "textview text { background-color: #fff5f7; }"
              "entry { background-color: #f8d7da; color: #4a4a4a; font-family: Fira Code, monospace; font-size: 12px; padding: 8px; border-radius: 5px; border: 1px solid #f5c6cb; transition: background-color 0.3s, color 0.3s; }"
              "label { color: #4a4a4a; font-family: Sans; font-size: 11px; transition: color 0.3s; }"
              "button { background-color: #e83e8c; color: #ffffff; border-radius: 5px; padding: 6px 12px; font-size: 11px; border: none; transition: background-color 0.3s; }"
              "button:hover { background-color: #c82333; }"
              ".blink { border: 2px solid red; }";
        gtk_button_set_label(GTK_BUTTON(term->theme_button), "Aydınlık");
        break;
    }

    gtk_css_provider_load_from_data(term->css_provider, css, -1, NULL);
}

static void toggle_theme(GtkWidget *widget, gpointer data)
{
    Terminal *term = (Terminal *)data;
    term->current_theme = (term->current_theme + 1) % 3;
    apply_css(term);
}

static void run_shell(int shell_num)
{
    gtk_init(NULL, NULL);

    char shell_name[MAX_NAME_LEN];
    snprintf(shell_name, MAX_NAME_LEN, "Terminal %d", shell_num);

    Terminal *term = malloc(sizeof(Terminal));
    if (!term)
    {
        fprintf(stderr, "Terminal oluşturulamadı!\n");
        exit(EXIT_FAILURE);
    }

    strncpy(term->shell_name, shell_name, MAX_NAME_LEN - 1);
    term->shell_name[MAX_NAME_LEN - 1] = '\0';
    term->cnt = 0;
    memset(term->history, 0, HISTORY_SIZE);
    memset(term->last_messages, 0, HISTORY_SIZE);
    term->notification_active = FALSE;
    term->current_theme = THEME_LIGHT;
    term->command_count = 0;
    term->current_command_index = -1;
    term->notification_blink_count = 0;
    memset(term->command_history, 0, sizeof(term->command_history));

    term->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(term->window), term->shell_name);
    gtk_window_set_default_size(GTK_WINDOW(term->window), 600, 400);
    gtk_window_set_position(GTK_WINDOW(term->window), GTK_WIN_POS_CENTER);
    gtk_window_move(GTK_WINDOW(term->window), 450 * (shell_num - 1), 0);
    g_signal_connect(term->window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    term->css_provider = gtk_css_provider_new();
    gtk_style_context_add_provider_for_screen(gdk_screen_get_default(),
                                              GTK_STYLE_PROVIDER(term->css_provider),
                                              GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(main_box), 8);

    // Başlık çubuğu (msg_label olmadan)
    GtkWidget *header_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_halign(header_box, GTK_ALIGN_FILL);
    GtkWidget *title_label = gtk_label_new(term->shell_name);
    gtk_box_pack_start(GTK_BOX(header_box), title_label, FALSE, FALSE, 8);
    term->theme_button = gtk_button_new_with_label("Karanlık");
    gtk_widget_set_size_request(term->theme_button, 100, 30);
    gtk_box_pack_end(GTK_BOX(header_box), term->theme_button, FALSE, FALSE, 8);
    g_signal_connect(term->theme_button, "clicked", G_CALLBACK(toggle_theme), term);
    gtk_box_pack_start(GTK_BOX(main_box), header_box, FALSE, FALSE, 0);

    GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(main_box), paned, TRUE, TRUE, 0);

    term->text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(term->text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(term->text_view), GTK_WRAP_WORD);
    GtkWidget *history_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(history_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(history_scrolled), term->text_view);
    gtk_paned_pack1(GTK_PANED(paned), history_scrolled, TRUE, FALSE);

    term->msg_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(term->msg_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(term->msg_view), GTK_WRAP_WORD);
    GtkWidget *msg_scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(msg_scrolled), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(msg_scrolled), term->msg_view);
    gtk_paned_pack2(GTK_PANED(paned), msg_scrolled, TRUE, FALSE);

    term->entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(term->entry), "Komut giriniz...");
    gtk_box_pack_start(GTK_BOX(main_box), term->entry, FALSE, FALSE, 8);
    g_signal_connect(term->entry, "key-press-event", G_CALLBACK(on_key_press), term);
    g_signal_connect(term->entry, "activate", G_CALLBACK(execute_command), term);

    gtk_container_add(GTK_CONTAINER(term->window), main_box);

    apply_css(term);

    g_timeout_add(1000, update_display, term);

    gtk_widget_show_all(term->window);
    gtk_main();

    g_object_unref(term->css_provider);
    free(term);
}

int main(int argc, char *argv[])
{
    global_shmp = buf_init();
    if (global_shmp == NULL)
    {
        fprintf(stderr, "Hata: Paylaşımlı bellek başlatılamadı!\n");
        return 1;
    }

    int num_shells = 2;
    if (argc > 1)
    {
        num_shells = atoi(argv[1]);
        if (num_shells <= 0 || num_shells > 10)
        {
            fprintf(stderr, "Geçersiz shell sayısı: %s. 1-10 arasında olmalı. Varsayılan 2 kullanılıyor.\n", argv[1]);
            num_shells = 2;
        }
    }

    printf("Açılacak shell sayısı: %d\n", num_shells);

    pid_t pids[num_shells];
    for (int i = 0; i < num_shells; i++)
    {
        pids[i] = fork();
        if (pids[i] == 0)
        {
            run_shell(i + 1);
            exit(EXIT_SUCCESS);
        }
        else if (pids[i] < 0)
        {
            errExit("fork error");
        }
        else
        {
            printf("Shell %d oluşturuldu, PID: %d\n", i + 1, pids[i]);
        }
    }

    for (int i = 0; i < num_shells; i++)
    {
        wait(NULL);
    }

    sem_close(global_shmp->sem);
    sem_unlink("/my_semaphore");
    munmap(global_shmp, sizeof(struct shmbuf));
    shm_unlink(SHARED_FILE_PATH);

    return 0;
}
