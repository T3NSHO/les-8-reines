#include <gtk/gtk.h>
#include <stdbool.h>
#include <stdlib.h>
#include <pthread.h>
#include <time.h>

typedef struct {
    GtkWidget *grid;
    GtkWidget *size_dropdown;
    GtkWidget *generate_button;
    GtkWidget *grid_container;
    GtkWidget *prev_button;
    GtkWidget *next_button;
    GtkWidget *operation_label;
    GtkWidget *solution_count_label;
    GtkWidget *method_dropdown;
    GtkWidget *overlay;
    int current_size;
    int **solutions;
    int solution_count;
    int current_solution_index;
    int operation_count;
    pthread_t solver_thread;
    bool thread_running;
    bool animation_running;
    guint animation_timer_id;
} GridData;

typedef struct {
    GridData *grid_data;
    int size;
    int method;
} ThreadData;

bool is_safe(int **board, int row, int col, int n) {
    for (int i = 0; i < col; i++) {
        if (board[row][i]) return false;
    }
    for (int i = row, j = col; i >= 0 && j >= 0; i--, j--) {
        if (board[i][j]) return false;
    }
    for (int i = row, j = col; i < n && j >= 0; i++, j--) {
        if (board[i][j]) return false;
    }
    return true;
}

bool is_configuration_valid(int **board, int n) {
    for (int col = 0; col < n; col++) {
        for (int row = 0; row < n; row++) {
            if (board[row][col] == 1) {
                for (int i = 0; i < n; i++) {
                    if (i != col && board[row][i] == 1) return false;
                    if (i != row && board[i][col] == 1) return false;
                }
                for (int i = 1; i < n; i++) {
                    if (row + i < n && col + i < n && board[row + i][col + i] == 1) return false;
                    if (row - i >= 0 && col - i >= 0 && board[row - i][col - i] == 1) return false;
                    if (row + i < n && col - i >= 0 && board[row + i][col - i] == 1) return false;
                    if (row - i >= 0 && col + i < n && board[row - i][col + i] == 1) return false;
                }
            }
        }
    }
    return true;
}

void generate_all_configurations(int **board, int col, int n, int *operation_count, int **solutions, int *solution_count) {
    if (col == n) {
        (*operation_count)++;
        if (is_configuration_valid(board, n)) {
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) {
                    solutions[*solution_count][i * n + j] = board[i][j];
                }
            }
            (*solution_count)++;
        }
        return;
    }

    for (int row = 0; row < n; row++) {
        board[row][col] = 1;
        generate_all_configurations(board, col + 1, n, operation_count, solutions, solution_count);
        board[row][col] = 0;
    }
}

bool solve_n_queens_intuitive(int **board, int col, int n, int *operation_count) {
    if (col == n) return true;
    for (int i = 0; i < n; i++) {
        (*operation_count)++;
        if (is_safe(board, i, col, n)) {
            board[i][col] = 1;
            if (solve_n_queens_intuitive(board, col + 1, n, operation_count)) {
                return true;
            }
            board[i][col] = 0;
        }
    }
    return false;
}

bool solve_n_queens_arborescent(int **board, int col, int n, int *operation_count, int **solutions, int *solution_count) {
    if (col == n) {
        (*operation_count)++;
        if (is_configuration_valid(board, n)) {
            for (int i = 0; i < n; i++) {
                for (int j = 0; j < n; j++) {
                    solutions[0][i * n + j] = board[i][j];
                }
            }
            *solution_count = 1;
            return true;
        }
        return false;
    }

    for (int row = 0; row < n; row++) {
        board[row][col] = 1;
        if (solve_n_queens_arborescent(board, col + 1, n, operation_count, solutions, solution_count)) {
            return true;
        }
        board[row][col] = 0;
    }
    return false;
}

static void update_grid_display(GridData *grid_data) {
    for (int i = 0; i < grid_data->current_size; i++) {
        for (int j = 0; j < grid_data->current_size; j++) {
            GtkWidget *cell = gtk_grid_get_child_at(GTK_GRID(grid_data->grid), j, i);
            if (cell) {
                if (grid_data->solutions[grid_data->current_solution_index][i * grid_data->current_size + j]) {
                    gtk_widget_set_name(cell, "queen-cell");
                } else {
                    gtk_widget_set_name(cell, "grid-cell");
                }
            }
        }
    }
}

static gboolean random_grid_animation(gpointer data) {
    GridData *grid_data = (GridData *)data;

    if (!grid_data->animation_running) {
        return G_SOURCE_REMOVE;
    }

    int row = rand() % grid_data->current_size;
    int col = rand() % grid_data->current_size;

    GtkWidget *cell = gtk_grid_get_child_at(GTK_GRID(grid_data->grid), col, row);

    if (cell == NULL) {
        return G_SOURCE_CONTINUE;
    }

    if (strcmp(gtk_widget_get_name(cell), "queen-cell") == 0) {
        gtk_widget_set_name(cell, "grid-cell");
    } else {
        gtk_widget_set_name(cell, "queen-cell");
    }

    return G_SOURCE_CONTINUE;
}

static gboolean update_ui_after_solve(gpointer data) {
    GridData *grid_data = (GridData *)data;

    grid_data->animation_running = FALSE;
    if (grid_data->animation_timer_id > 0) {
        g_source_remove(grid_data->animation_timer_id);
        grid_data->animation_timer_id = 0;
    }

    char operation_text[50];
    snprintf(operation_text, sizeof(operation_text), "Operations: %d", grid_data->operation_count);
    gtk_label_set_text(GTK_LABEL(grid_data->operation_label), operation_text);

    char solution_count_text[50];
    snprintf(solution_count_text, sizeof(solution_count_text), "Solutions: %d", grid_data->solution_count);
    gtk_label_set_text(GTK_LABEL(grid_data->solution_count_label), solution_count_text);

    if (grid_data->solution_count > 0) {
        grid_data->current_solution_index = 0;
        update_grid_display(grid_data);
    }

    gtk_widget_set_sensitive(grid_data->generate_button, TRUE);

    grid_data->thread_running = FALSE;
    return G_SOURCE_REMOVE;
}

static void* solver_thread_func(void *arg) {
    ThreadData *thread_data = (ThreadData *)arg;
    GridData *grid_data = thread_data->grid_data;
    int size = thread_data->size;
    int method = thread_data->method;

    int **board = malloc(size * sizeof(int *));
    for (int i = 0; i < size; i++) {
        board[i] = calloc(size, sizeof(int));
    }

    switch (method) {
        case 0:
            if (solve_n_queens_intuitive(board, 0, size, &grid_data->operation_count)) {
                grid_data->solution_count = 1;
                for (int i = 0; i < size; i++) {
                    for (int j = 0; j < size; j++) {
                        grid_data->solutions[0][i * size + j] = board[i][j];
                    }
                }
            }
            break;
        case 1:
            solve_n_queens_arborescent(board, 0, size, &grid_data->operation_count, grid_data->solutions, &grid_data->solution_count);
            break;
        case 2:
            generate_all_configurations(board, 0, size, &grid_data->operation_count, grid_data->solutions, &grid_data->solution_count);
            break;
    }

    for (int i = 0; i < size; i++) {
        free(board[i]);
    }
    free(board);

    g_idle_add((GSourceFunc)update_ui_after_solve, grid_data);
    
    free(thread_data);
    return NULL;
}

static void generate_grid(GtkWidget *widget, gpointer data) {
    GridData *grid_data = (GridData *)data;
    
    if (grid_data->thread_running) return;
    
    int size = gtk_drop_down_get_selected(GTK_DROP_DOWN(grid_data->size_dropdown)) + 3;
    int method = gtk_drop_down_get_selected(GTK_DROP_DOWN(grid_data->method_dropdown));

    if (grid_data->grid) {
        gtk_widget_unparent(grid_data->grid);
    }

    grid_data->grid = gtk_grid_new();
    gtk_widget_set_name(grid_data->grid, "chess-grid");
    gtk_grid_set_row_homogeneous(GTK_GRID(grid_data->grid), TRUE);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid_data->grid), TRUE);

    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            GtkWidget *cell = gtk_button_new();
            gtk_widget_set_name(cell, "grid-cell");
            gtk_widget_set_size_request(cell, 50, 50);
            gtk_grid_attach(GTK_GRID(grid_data->grid), cell, j, i, 1, 1);
        }
    }

    gtk_box_append(GTK_BOX(grid_data->grid_container), grid_data->grid);
    gtk_widget_set_visible(grid_data->grid, TRUE);

    if (grid_data->solutions) {
        for (int i = 0; i < grid_data->solution_count; i++) {
            free(grid_data->solutions[i]);
        }
        free(grid_data->solutions);
    }

    grid_data->solutions = malloc(1000 * sizeof(int *));
    for (int i = 0; i < 1000; i++) {
        grid_data->solutions[i] = malloc(size * size * sizeof(int));
    }

    grid_data->solution_count = 0;
    grid_data->operation_count = 0;
    grid_data->current_size = size;

    grid_data->animation_running = TRUE;
    grid_data->animation_timer_id = g_timeout_add(100, random_grid_animation, grid_data);

    gtk_widget_set_sensitive(grid_data->generate_button, FALSE);

    ThreadData *thread_data = malloc(sizeof(ThreadData));
    thread_data->grid_data = grid_data;
    thread_data->size = size;
    thread_data->method = method;
    
    grid_data->thread_running = TRUE;
    pthread_create(&grid_data->solver_thread, NULL, solver_thread_func, thread_data);
    pthread_detach(grid_data->solver_thread);
}

static void show_next_solution(GtkWidget *widget, gpointer data) {
    GridData *grid_data = (GridData *)data;
    if (grid_data->solution_count == 0) return;

    grid_data->current_solution_index = (grid_data->current_solution_index + 1) % grid_data->solution_count;
    update_grid_display(grid_data);
}

static void show_previous_solution(GtkWidget *widget, gpointer data) {
    GridData *grid_data = (GridData *)data;
    if (grid_data->solution_count == 0) return;

    grid_data->current_solution_index = (grid_data->current_solution_index - 1 + grid_data->solution_count) % grid_data->solution_count;
    update_grid_display(grid_data);
}

static void switch_to_solver(GtkWidget *widget, gpointer data) {
    GtkStack *stack = GTK_STACK(data);
    gtk_stack_set_visible_child_name(stack, "solver");
}

static void switch_to_main_menu(GtkWidget *widget, gpointer data) {
    GtkStack *stack = GTK_STACK(data);
    gtk_stack_set_visible_child_name(stack, "main");
}

static void switch_to_about(GtkWidget *widget, gpointer data) {
    GtkStack *stack = GTK_STACK(data);
    gtk_stack_set_visible_child_name(stack, "about");
}

static void draw_background(GtkDrawingArea *area, cairo_t *cr, int width, int height, gpointer data) {
    GdkPixbuf *original_pixbuf = data;
    if (original_pixbuf) {
        GdkPixbuf *scaled_pixbuf = gdk_pixbuf_scale_simple(original_pixbuf, width, height, GDK_INTERP_BILINEAR);
        gdk_cairo_set_source_pixbuf(cr, scaled_pixbuf, 0, 0);
        cairo_paint(cr);
        g_object_unref(scaled_pixbuf);
    }
}

static GtkWidget *create_developer_card(const char *name, const char *role, const char *image_path) {
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_size_request(card, 250, 300);
    gtk_widget_add_css_class(card, "developer-card");
    gtk_widget_set_halign(card, GTK_ALIGN_CENTER);
    
    GtkWidget *avatar = gtk_image_new_from_file(image_path);
    gtk_image_set_pixel_size(GTK_IMAGE(avatar), 200);
    gtk_widget_set_margin_top(avatar, 20);
    
    GtkWidget *name_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(name_label), 
        g_markup_printf_escaped("<span font='16' weight='bold' foreground='#fde68a'>%s</span>", name));
    gtk_widget_set_margin_top(name_label, 10);
    
    GtkWidget *role_label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(role_label),
        g_markup_printf_escaped("<span foreground='white'>%s</span>", role));
    
    gtk_box_append(GTK_BOX(card), avatar);
    gtk_box_append(GTK_BOX(card), name_label);
    gtk_box_append(GTK_BOX(card), role_label);
    
    return card;
}

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "8 Reines Solver");
    gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);
    gtk_window_set_icon_name(GTK_WINDOW(window), "logo.png");

    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_data(provider,
    "window { background-color: transparent; }"
    "#title { font-size: 64px; font-weight: bold; color: #D4AF37; margin-bottom: 10px; }"
    "#subtitle { font-size: 36px; font-weight: bold; color: white; margin-top: -10px; }"
    "#solve-button { background: transparent; color: #D4AF37; border: 2px solid #D4AF37; border-radius: 25px; padding: 15px 30px; font-size: 20px; font-weight: bold; min-width: 150px; max-width: 150px; transition: transform 0.3s, background-color 0.3s; }"
    "#solve-button:hover { background-color: rgba(212, 175, 55, 0.1); transform: scale(1.1); }"
    "#input-label { color: #D4AF37; font-size: 18px; font-weight: bold; }"
    "#size-dropdown { background: rgba(255, 255, 255, 0.1); color: white; border: 2px solid #D4AF37; border-radius: 5px; padding: 5px; transition: transform 0.2s; }"
    "#size-dropdown:hover { transform: scale(1.05); }"
    "#method-dropdown { background: rgba(255, 255, 255, 0.1); color: white; border: 2px solid #D4AF37; border-radius: 5px; padding: 5px; transition: transform 0.2s; }"
    "#method-dropdown:hover { transform: scale(1.05); }"
    "#generate-button { background: transparent; color: #D4AF37; border: 2px solid #D4AF37; border-radius: 25px; padding: 10px 20px; font-size: 16px; font-weight: bold; transition: transform 0.3s, background-color 0.3s; }"
    "#generate-button:hover { background-color: rgba(212, 175, 55, 0.1); transform: scale(1.1); }"
    "#chess-grid { background: transparent; }"
    "#grid-cell { background: #E6E6FA; min-width: 50px; min-height: 50px; transition: background-color 0.3s; }"
    "#queen-cell { background: #000000; min-width: 50px; min-height: 50px; transition: background-color 0.3s; }"
    "#back-button { background: transparent; color: #FF0000; border: 2px solid #FF0000; border-radius: 25px; padding: 10px 20px; font-size: 16px; font-weight: bold; transition: transform 0.3s, background-color 0.3s; }"
    "#back-button:hover { background-color: rgba(255, 0, 0, 0.1); transform: scale(1.1); }"
    "#close-button { background: transparent; color: #FF0000; border: 2px solid #FF0000; border-radius: 25px; padding: 10px 20px; font-size: 16px; font-weight: bold; transition: transform 0.3s, background-color 0.3s; }"
    "#close-button:hover { background-color: rgba(255, 0, 0, 0.1); transform: scale(1.1); }"
    "#about-button { background: transparent; color: #D4AF37; border: 2px solid #D4AF37; border-radius: 25px; padding: 10px 20px; font-size: 16px; font-weight: bold; transition: transform 0.3s, background-color 0.3s; }"
    "#about-button:hover { background-color: rgba(212, 175, 55, 0.1); transform: scale(1.1); }"
    "#nav-button { background: transparent; color: #D4AF37; border: 2px solid #D4AF37; border-radius: 25px; padding: 10px 20px; font-size: 16px; font-weight: bold; transition: transform 0.3s, background-color 0.3s; }"
    "#nav-button:hover { background-color: rgba(212, 175, 55, 0.1); transform: scale(1.1); }"
    "#operation-label { color: #D4AF37; font-size: 18px; font-weight: bold; }"
    "#solution-count-label { color: #D4AF37; font-size: 18px; font-weight: bold; }"
    ".developer-card { "
    "  background: rgba(88, 28, 135, 0.3); "
    "  border-radius: 24px; "
    "  padding: 20px; "
    "  margin: 10px; "
    "  transition: transform 0.3s; "
    "} "
    ".developer-card:hover { transform: scale(1.05); }", -1);
    gtk_style_context_add_provider_for_display(gdk_display_get_default(), GTK_STYLE_PROVIDER(provider), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    GtkWidget *overlay = gtk_overlay_new();
    GtkWidget *drawing_area = gtk_drawing_area_new();

    GError *error = NULL;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file("background.png", &error);
    if (error) {
        g_warning("Failed to load background: %s", error->message);
        g_error_free(error);
    } else {
        gtk_drawing_area_set_draw_func(GTK_DRAWING_AREA(drawing_area), draw_background, pixbuf, g_object_unref);
    }

    g_signal_connect(drawing_area, "resize", G_CALLBACK(gtk_widget_queue_draw), NULL);

    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);

    GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *top_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(top_bar, GTK_ALIGN_END);
    GtkWidget *logo_image = gtk_image_new_from_file("logo.png");
    gtk_image_set_pixel_size(GTK_IMAGE(logo_image), 150);
    gtk_box_append(GTK_BOX(top_bar), logo_image);
    gtk_widget_set_margin_top(top_bar, 10);
    gtk_widget_set_margin_end(top_bar, 10);

    GtkWidget *center_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_vexpand(center_box, TRUE);
    gtk_widget_set_valign(center_box, GTK_ALIGN_CENTER);

    GtkWidget *title = gtk_label_new("8 REINES SOLVER");
    gtk_widget_set_name(title, "title");
    GtkWidget *subtitle = gtk_label_new("GAME");
    gtk_widget_set_name(subtitle, "subtitle");
    GtkWidget *solve_button = gtk_button_new_with_label("SOLVE");
    gtk_widget_set_name(solve_button, "solve-button");

    g_signal_connect(solve_button, "clicked", G_CALLBACK(switch_to_solver), stack);

    gtk_box_append(GTK_BOX(center_box), title);
    gtk_box_append(GTK_BOX(center_box), subtitle);
    gtk_box_append(GTK_BOX(center_box), solve_button);

    GtkWidget *bottom_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_margin_bottom(bottom_bar, 20);
    gtk_widget_set_hexpand(bottom_bar, TRUE);

    GtkWidget *close_button = gtk_button_new_with_label("CLOSE");
    gtk_widget_set_name(close_button, "close-button");
    gtk_widget_set_margin_start(close_button, 20);
    g_signal_connect(close_button, "clicked", G_CALLBACK(gtk_window_destroy), window);

    GtkWidget *about_button = gtk_button_new_with_label("ABOUT");
    gtk_widget_set_name(about_button, "about-button");
    gtk_widget_set_margin_end(about_button, 20);
    gtk_widget_set_margin_start(about_button, 1700);
    gtk_widget_set_halign(about_button, GTK_ALIGN_END);
    gtk_widget_set_valign(about_button, GTK_ALIGN_END);
    g_signal_connect(about_button, "clicked", G_CALLBACK(switch_to_about), stack);

    gtk_box_append(GTK_BOX(bottom_bar), close_button);
    gtk_box_append(GTK_BOX(bottom_bar), about_button);

    gtk_box_append(GTK_BOX(main_box), top_bar);
    gtk_box_append(GTK_BOX(main_box), center_box);
    gtk_box_append(GTK_BOX(main_box), bottom_bar);

    gtk_stack_add_named(GTK_STACK(stack), main_box, "main");

    GtkWidget *solver_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_margin_start(solver_box, 20);
    gtk_widget_set_margin_end(solver_box, 20);
    gtk_widget_set_margin_top(solver_box, 20);
    gtk_widget_set_margin_bottom(solver_box, 20);

    GtkWidget *input_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(input_box, GTK_ALIGN_CENTER);

    GtkWidget *size_label = gtk_label_new("Taille de la grille (3-10):");
    gtk_widget_set_name(size_label, "input-label");

    GtkStringList *size_list = gtk_string_list_new(NULL);
    for (int i = 3; i <= 10; i++) {
        char buffer[10];
        snprintf(buffer, sizeof(buffer), "%d", i);
        gtk_string_list_append(size_list, buffer);
    }

    GtkWidget *size_dropdown = gtk_drop_down_new(G_LIST_MODEL(size_list), NULL);
    gtk_widget_set_name(size_dropdown, "size-dropdown");

    GtkStringList *method_list = gtk_string_list_new(NULL);
    gtk_string_list_append(method_list, "Recherche intuitive");
    gtk_string_list_append(method_list, "Exploration arborescente");
    gtk_string_list_append(method_list, "Exploration exhaustive");

    GtkWidget *method_dropdown = gtk_drop_down_new(G_LIST_MODEL(method_list), NULL);
    gtk_widget_set_name(method_dropdown, "method-dropdown");

    GtkWidget *generate_button = gtk_button_new_with_label("Générer la grille");
    gtk_widget_set_name(generate_button, "generate-button");

    gtk_box_append(GTK_BOX(input_box), size_label);
    gtk_box_append(GTK_BOX(input_box), size_dropdown);
    gtk_box_append(GTK_BOX(input_box), method_dropdown);
    gtk_box_append(GTK_BOX(input_box), generate_button);

    GtkWidget *grid_container = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(grid_container, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(grid_container, GTK_ALIGN_CENTER);
    gtk_widget_set_vexpand(grid_container, TRUE);

    GtkWidget *nav_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(nav_box, GTK_ALIGN_CENTER);

    GtkWidget *prev_button = gtk_button_new_with_label("Solution précédente");
    gtk_widget_set_name(prev_button, "nav-button");

    GtkWidget *next_button = gtk_button_new_with_label("Solution suivante");
    gtk_widget_set_name(next_button, "nav-button");

    GtkWidget *operation_label = gtk_label_new("Opérations: 0");
    gtk_widget_set_name(operation_label, "operation-label");

    GtkWidget *solution_count_label = gtk_label_new("Solutions: 0");
    gtk_widget_set_name(solution_count_label, "solution-count-label");

    gtk_box_append(GTK_BOX(nav_box), prev_button);
    gtk_box_append(GTK_BOX(nav_box), next_button);
    gtk_box_append(GTK_BOX(nav_box), operation_label);
    gtk_box_append(GTK_BOX(nav_box), solution_count_label);

    GtkWidget *back_button = gtk_button_new_with_label("RETOUR");
    gtk_widget_set_name(back_button, "back-button");
    gtk_widget_set_halign(back_button, GTK_ALIGN_START);
    g_signal_connect(back_button, "clicked", G_CALLBACK(switch_to_main_menu), stack);

    gtk_box_append(GTK_BOX(solver_box), input_box);
    gtk_box_append(GTK_BOX(solver_box), grid_container);
    gtk_box_append(GTK_BOX(solver_box), nav_box);
    gtk_box_append(GTK_BOX(solver_box), back_button);

    gtk_stack_add_named(GTK_STACK(stack), solver_box, "solver");

    GtkWidget *about_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 30);
    gtk_widget_set_margin_start(about_box, 40);
    gtk_widget_set_margin_end(about_box, 40);
    gtk_widget_set_margin_top(about_box, 40);
    gtk_widget_set_margin_bottom(about_box, 40);

    GtkWidget *about_title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(about_title), "<span font='48' weight='bold' foreground='#fde68a'>Developers</span>");
    gtk_widget_set_margin_bottom(about_title, 40);
    gtk_box_append(GTK_BOX(about_box), about_title);

    GtkWidget *cards_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 20);
    gtk_widget_set_halign(cards_box, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(cards_box, 40);

    const char *developers[][3] = {
        {"HASSOUNE ZAKARIA", "TESTER DEVELOPERS", "avatar1.png"},
        {"AZOUGEN ZINEB", "TESTER DEVELOPERS", "avatar2.png"},
        {"OUKHRID MOHAMED AMINE", "DESIGNER DEVELOPERS", "avatar3.png"},
        {"ABOUHAFSS OUSSAMA", "SCRUM MA DEVELOPERS", "avatar4.png"},
        {"AZEMRAY OUALID", "TESTER DEVELOPERS", "avatar5.png"}
    };

    for (int i = 0; i < 5; i++) {
        GtkWidget *card = create_developer_card(
            developers[i][0], 
            developers[i][1],
            developers[i][2]
        );
        gtk_box_append(GTK_BOX(cards_box), card);
    }
    gtk_box_append(GTK_BOX(about_box), cards_box);

    GtkWidget *back_button_about = gtk_button_new_with_label("BACK");
    gtk_widget_set_name(back_button_about, "back-button");
    gtk_widget_set_halign(back_button_about, GTK_ALIGN_START);
    gtk_widget_set_valign(back_button_about, GTK_ALIGN_END);
    gtk_widget_set_margin_start(back_button_about, 20);
    gtk_widget_set_margin_bottom(back_button_about, 20);
    gtk_box_append(GTK_BOX(about_box), back_button_about);
    g_signal_connect(back_button_about, "clicked", G_CALLBACK(switch_to_main_menu), stack);

    // Add the about_box directly to the stack
    gtk_stack_add_named(GTK_STACK(stack), about_box, "about");

    gtk_overlay_set_child(GTK_OVERLAY(overlay), drawing_area);
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), stack);

    gtk_window_set_child(GTK_WINDOW(window), overlay);

    GridData *grid_data = g_new(GridData, 1);
    grid_data->grid = NULL;
    grid_data->size_dropdown = size_dropdown;
    grid_data->generate_button = generate_button;
    grid_data->grid_container = grid_container;
    grid_data->prev_button = prev_button;
    grid_data->next_button = next_button;
    grid_data->operation_label = operation_label;
    grid_data->solution_count_label = solution_count_label;
    grid_data->method_dropdown = method_dropdown;
    grid_data->overlay = overlay;
    grid_data->current_size = 0;
    grid_data->solutions = NULL;
    grid_data->solution_count = 0;
    grid_data->current_solution_index = 0;
    grid_data->operation_count = 0;
    grid_data->thread_running = false;
    grid_data->animation_running = false;
    grid_data->animation_timer_id = 0;

    g_signal_connect(generate_button, "clicked", G_CALLBACK(generate_grid), grid_data);
    g_signal_connect(prev_button, "clicked", G_CALLBACK(show_previous_solution), grid_data);
    g_signal_connect(next_button, "clicked", G_CALLBACK(show_next_solution), grid_data);

    gtk_window_present(GTK_WINDOW(window));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.example.solver", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    return status;
}