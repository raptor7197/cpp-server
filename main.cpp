#include <adwaita.h>
#include <gtk/gtk.h>
#include <string>
#include <cstring>
#include <cmath>
#include <vector>
#include <filesystem>

// =============================================================================
//  Pure C++ Desktop UI using GTK4 + libadwaita
//
//  No HTML. No CSS. No JavaScript. No web server.
//  Everything is a native GTK4/Adw widget built from C++ code.
//
//  Animations:
//    - Hover scale effect on section boxes (GtkEventControllerMotion + AdwTimedAnimation)
//    - Slide-in reveal for the toggle section (AdwTimedAnimation on margin/opacity)
//    - Button press pop animation (AdwTimedAnimation scale bounce)
//    - Sections slide in on window load (staggered AdwTimedAnimation)
//
//  Compile with:
//    g++ main.cpp -o app $(pkg-config --cflags --libs libadwaita-1) -std=c++17 && ./app
// =============================================================================

// ---- Global state for interactions ----
static int click_count = 0;
static GtkWidget *counter_label = nullptr;
static GtkWidget *hidden_box_revealer = nullptr;
static bool hidden_visible = false;
static GtkWidget *mirror_label = nullptr;
static GtkWidget *main_window = nullptr;

// Store section boxes for entrance animation
static std::vector<GtkWidget *> section_widgets;

// ---- DOM Playground state ----
static int dom_element_id = 0;                        // auto-increment ID for each created element
static GtkWidget *dom_container = nullptr;            // the box that holds dynamically created children
static GtkWidget *dom_count_label = nullptr;          // label showing current child count
static std::vector<GtkWidget *> dom_children;         // tracks all live child widgets

// ---- Image Gallery state ----
static GtkWidget *img_container = nullptr;            // the box that holds image widgets
static GtkWidget *img_count_label = nullptr;          // label showing image count
static std::vector<GtkWidget *> img_children;         // tracks all live image card widgets
static int img_element_id = 0;                        // auto-increment ID for each image

// =============================================================================
//  Animation data structures
// =============================================================================

// Data passed to hover animation tick
struct HoverAnimData {
    GtkWidget *widget;
    double     current_scale;
    double     target_scale;
};

// Data for slide-reveal animation
struct SlideAnimData {
    GtkWidget *widget;
    bool       revealing; // true = sliding in, false = sliding out
};

// =============================================================================
//  HOVER ANIMATION — scale a widget up/down on mouse enter/leave
//  Uses AdwTimedAnimation driving a custom callback that applies a
//  GskTransform (scale) to the widget via gtk_widget_set_opacity +
//  margin trick for visual feedback (GTK4 doesn't expose raw transforms
//  easily, so we animate margins to create a "grow" effect and opacity
//  for a glow feel).
// =============================================================================

static void hover_anim_value_cb(double value, gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    // value goes from 0.0 to 1.0 (enter) or 1.0 to 0.0 (leave)
    // We map this to margin reduction (simulating scale-up) and slight opacity boost
    int margin = (int)(12.0 * (1.0 - value));    // 12 -> 0 on hover
    int padding_extra = (int)(4.0 * value);       // 0 -> 4 on hover

    gtk_widget_set_margin_start(widget, margin);
    gtk_widget_set_margin_end(widget, margin);
    gtk_widget_set_margin_top(widget, padding_extra > 0 ? 2 : 4);
    gtk_widget_set_margin_bottom(widget, padding_extra > 0 ? 2 : 4);

    // Slight opacity pulse: 0.85 -> 1.0
    double opacity = 0.85 + 0.15 * value;
    gtk_widget_set_opacity(widget, opacity);
}

static void attach_hover_animation(GtkWidget *widget) {
    // Start with base margins and slightly dimmed
    gtk_widget_set_margin_start(widget, 12);
    gtk_widget_set_margin_end(widget, 12);
    gtk_widget_set_opacity(widget, 0.85);

    // Create motion controller for hover detection
    GtkEventController *motion = gtk_event_controller_motion_new();

    // Mouse enter: animate value from 0 -> 1
    g_signal_connect(motion, "enter",
        G_CALLBACK(+[](GtkEventControllerMotion * /*ctrl*/, double /*x*/, double /*y*/, gpointer user_data) {
            GtkWidget *w = GTK_WIDGET(user_data);
            AdwAnimationTarget *target = adw_callback_animation_target_new(
                hover_anim_value_cb, w, nullptr);
            AdwAnimation *anim = adw_timed_animation_new(
                w,     // widget
                0.0,   // value-from
                1.0,   // value-to
                200,   // duration ms
                target  // target
            );
            adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_OUT_CUBIC);
            adw_animation_play(anim);
        }), widget);

    // Mouse leave: animate value from 1 -> 0
    g_signal_connect(motion, "leave",
        G_CALLBACK(+[](GtkEventControllerMotion * /*ctrl*/, gpointer user_data) {
            GtkWidget *w = GTK_WIDGET(user_data);
            AdwAnimationTarget *target = adw_callback_animation_target_new(
                hover_anim_value_cb, w, nullptr);
            AdwAnimation *anim = adw_timed_animation_new(
                w,
                1.0,   // value-from
                0.0,   // value-to
                300,   // duration ms (slower leave for smoothness)
                target
            );
            adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_IN_OUT_CUBIC);
            adw_animation_play(anim);
        }), widget);

    gtk_widget_add_controller(widget, motion);
}

// =============================================================================
//  SLIDE REVEAL ANIMATION — smoothly slide hidden content in/out
//  Animates both margin-top (slide) and opacity (fade) simultaneously
// =============================================================================

static void slide_reveal_value_cb(double value, gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    // value: 0.0 = fully hidden, 1.0 = fully visible
    int slide_offset = (int)(40.0 * (1.0 - value)); // slides down from 40px above
    gtk_widget_set_margin_top(widget, slide_offset);
    gtk_widget_set_opacity(widget, value);

    if (value <= 0.01) {
        gtk_widget_set_visible(widget, FALSE);
    }
}

static void animate_slide_in(GtkWidget *widget) {
    gtk_widget_set_visible(widget, TRUE);
    gtk_widget_set_opacity(widget, 0.0);
    gtk_widget_set_margin_top(widget, 40);

    AdwAnimationTarget *target = adw_callback_animation_target_new(
        slide_reveal_value_cb, widget, nullptr);
    AdwAnimation *anim = adw_timed_animation_new(
        widget, 0.0, 1.0, 350, target);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_OUT_BACK);
    adw_animation_play(anim);
}

static void animate_slide_out(GtkWidget *widget) {
    AdwAnimationTarget *target = adw_callback_animation_target_new(
        slide_reveal_value_cb, widget, nullptr);
    AdwAnimation *anim = adw_timed_animation_new(
        widget, 1.0, 0.0, 250, target);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_IN_CUBIC);
    adw_animation_play(anim);
}

// =============================================================================
//  BUTTON POP ANIMATION — quick scale bounce when a button is clicked
// =============================================================================

static void button_pop_value_cb(double value, gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    // value goes 0 -> 1: we map it to a bounce curve via margins
    // At value=0.5 the button is "squished" (extra margin), then back to normal
    double squeeze = sin(value * G_PI) * 3.0;
    gtk_widget_set_margin_top(widget, (int)squeeze);
    gtk_widget_set_margin_start(widget, (int)(squeeze * 0.5));
}

static void animate_button_pop(GtkWidget *button) {
    AdwAnimationTarget *target = adw_callback_animation_target_new(
        button_pop_value_cb, button, nullptr);
    AdwAnimation *anim = adw_timed_animation_new(
        button, 0.0, 1.0, 200, target);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_OUT_BOUNCE);
    adw_animation_play(anim);
}

// =============================================================================
//  ENTRANCE ANIMATION — staggered slide-in for each section on app start
// =============================================================================

static void entrance_value_cb(double value, gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    int offset = (int)(60.0 * (1.0 - value));
    gtk_widget_set_margin_top(widget, offset + 4);
    gtk_widget_set_opacity(widget, value);
}

static gboolean entrance_start_one(gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    AdwAnimationTarget *target = adw_callback_animation_target_new(
        entrance_value_cb, widget, nullptr);
    AdwAnimation *anim = adw_timed_animation_new(
        widget, 0.0, 1.0, 500, target);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_OUT_CUBIC);
    adw_animation_play(anim);
    return G_SOURCE_REMOVE; // one-shot timer
}

static void schedule_entrance_animations() {
    for (size_t i = 0; i < section_widgets.size(); i++) {
        GtkWidget *w = section_widgets[i];
        gtk_widget_set_opacity(w, 0.0);
        gtk_widget_set_margin_top(w, 60);
        // Stagger each section by 120ms
        g_timeout_add((guint)(100 + i * 120), entrance_start_one, w);
    }
}

// =============================================================================
//  DOM Playground — dynamic element creation and deletion
// =============================================================================

// Updates the "X elements in container" count label
static void dom_update_count() {
    std::string text = std::to_string(dom_children.size()) + " element(s) in container";
    gtk_label_set_text(GTK_LABEL(dom_count_label), text.c_str());
}

// Data passed to the delete-slide-out animation so we can remove the widget after it finishes
struct DomRemoveData {
    GtkWidget *card;
};

// Slide-out callback for deleting an element
static void dom_remove_value_cb(double value, gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    // value: 1.0 -> 0.0  (fading + sliding out to the right)
    int offset = (int)(80.0 * (1.0 - value));
    gtk_widget_set_margin_start(widget, offset);
    gtk_widget_set_opacity(widget, value);
}

// Called when the slide-out animation finishes — actually removes the widget
static void dom_remove_done_cb(AdwAnimation * /*anim*/, gpointer user_data) {
    auto *data = static_cast<DomRemoveData *>(user_data);
    GtkWidget *card = data->card;

    // Remove from our tracking vector
    for (auto it = dom_children.begin(); it != dom_children.end(); ++it) {
        if (*it == card) {
            dom_children.erase(it);
            break;
        }
    }

    // Remove from the container (destroys the widget)
    gtk_box_remove(GTK_BOX(dom_container), card);
    dom_update_count();
    delete data;
}

// Callback: "Delete" button on an individual card — slides it out then removes it
static void on_dom_delete_clicked(GtkButton *button, gpointer user_data) {
    GtkWidget *card = GTK_WIDGET(user_data);
    animate_button_pop(GTK_WIDGET(button));

    // Animate slide-out, then remove on completion
    AdwAnimationTarget *target = adw_callback_animation_target_new(
        dom_remove_value_cb, card, nullptr);
    AdwAnimation *anim = adw_timed_animation_new(
        card, 1.0, 0.0, 300, target);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_IN_CUBIC);

    // When animation finishes, destroy the widget
    auto *remove_data = new DomRemoveData{card};
    g_signal_connect(anim, "done", G_CALLBACK(dom_remove_done_cb), remove_data);

    adw_animation_play(anim);
}

// Slide-in callback for a newly added element
static void dom_add_value_cb(double value, gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    // value: 0.0 -> 1.0  (fading + sliding in from the left)
    int offset = (int)(60.0 * (1.0 - value));
    gtk_widget_set_margin_start(widget, offset);
    gtk_widget_set_opacity(widget, value);
}

// Callback: "Add Element" button — creates a new card, slides it in
static void on_dom_add_clicked(GtkButton *button, gpointer /*user_data*/) {
    animate_button_pop(GTK_WIDGET(button));
    dom_element_id++;

    // ---- Build a card (horizontal box): [label  |  delete button] ----
    GtkWidget *card = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_top(card, 2);
    gtk_widget_set_margin_bottom(card, 2);

    // Element label with ID and timestamp
    std::string label_text = "Element #" + std::to_string(dom_element_id);
    GtkWidget *elabel = gtk_label_new(nullptr);
    std::string markup = "<span weight='bold'>" + label_text + "</span>"
                         "  —  created dynamically from C++";
    gtk_label_set_markup(GTK_LABEL(elabel), markup.c_str());
    gtk_label_set_xalign(GTK_LABEL(elabel), 0.0f);
    gtk_widget_set_hexpand(elabel, TRUE);
    gtk_box_append(GTK_BOX(card), elabel);

    // Delete button (removes this specific card)
    GtkWidget *del_btn = gtk_button_new_with_label("Delete");
    gtk_widget_set_halign(del_btn, GTK_ALIGN_END);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_dom_delete_clicked), card);
    gtk_box_append(GTK_BOX(card), del_btn);

    // Append to container
    gtk_box_append(GTK_BOX(dom_container), card);
    dom_children.push_back(card);
    dom_update_count();

    // Slide-in animation
    gtk_widget_set_opacity(card, 0.0);
    gtk_widget_set_margin_start(card, 60);
    AdwAnimationTarget *target = adw_callback_animation_target_new(
        dom_add_value_cb, card, nullptr);
    AdwAnimation *anim = adw_timed_animation_new(
        card, 0.0, 1.0, 350, target);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_OUT_BACK);
    adw_animation_play(anim);
}

// Callback: "Clear All" button — removes every child with a staggered slide-out
static void on_dom_clear_clicked(GtkButton *button, gpointer /*user_data*/) {
    animate_button_pop(GTK_WIDGET(button));

    // Copy the vector because callbacks will mutate it
    std::vector<GtkWidget *> to_remove = dom_children;

    for (size_t i = 0; i < to_remove.size(); i++) {
        GtkWidget *card = to_remove[i];

        AdwAnimationTarget *target = adw_callback_animation_target_new(
            dom_remove_value_cb, card, nullptr);
        AdwAnimation *anim = adw_timed_animation_new(
            card, 1.0, 0.0, 250, target);
        adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_IN_CUBIC);

        auto *remove_data = new DomRemoveData{card};
        g_signal_connect(anim, "done", G_CALLBACK(dom_remove_done_cb), remove_data);

        // Stagger each removal by 80ms
        g_timeout_add((guint)(i * 80),
            +[](gpointer data) -> gboolean {
                adw_animation_play(ADW_ANIMATION(data));
                return G_SOURCE_REMOVE;
            }, anim);
    }
}

// =============================================================================
//  Interaction callbacks (with animations added)
// =============================================================================

static void on_counter_clicked(GtkButton *button, gpointer /*user_data*/) {
    click_count++;
    std::string text = "You have clicked: " + std::to_string(click_count) + " times";
    gtk_label_set_text(GTK_LABEL(counter_label), text.c_str());
    animate_button_pop(GTK_WIDGET(button));
}

static void on_toggle_clicked(GtkButton *button, gpointer /*user_data*/) {
    hidden_visible = !hidden_visible;
    if (hidden_visible) {
        animate_slide_in(hidden_box_revealer);
    } else {
        animate_slide_out(hidden_box_revealer);
    }
    animate_button_pop(GTK_WIDGET(button));
}

static void on_entry_changed(GtkEditable *editable, gpointer /*user_data*/) {
    const char *typed = gtk_editable_get_text(editable);
    if (typed && strlen(typed) > 0) {
        gtk_label_set_text(GTK_LABEL(mirror_label), typed);
    } else {
        gtk_label_set_text(GTK_LABEL(mirror_label), "(type something above)");
    }
}

static void on_reset_clicked(GtkButton *button, gpointer entry) {
    click_count = 0;
    gtk_label_set_text(GTK_LABEL(counter_label), "You have clicked: 0 times");
    gtk_editable_set_text(GTK_EDITABLE(entry), "");
    gtk_label_set_text(GTK_LABEL(mirror_label), "(type something above)");
    if (hidden_visible) {
        hidden_visible = false;
        animate_slide_out(hidden_box_revealer);
    }
    animate_button_pop(GTK_WIDGET(button));
}

// =============================================================================
//  Helper functions to create widgets
// =============================================================================

static GtkWidget *make_heading(const char *text, int level) {
    GtkWidget *label = gtk_label_new(nullptr);
    const char *size_tag = "xx-large";
    if (level == 2) size_tag = "x-large";
    else if (level >= 3) size_tag = "large";

    std::string markup = std::string("<span size='") + size_tag + "' weight='bold'>" + text + "</span>";
    gtk_label_set_markup(GTK_LABEL(label), markup.c_str());
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    return label;
}

static GtkWidget *make_paragraph(const char *text) {
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    return label;
}

static GtkWidget *make_separator() {
    return gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
}

static GtkWidget *make_box(int spacing) {
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing);
    return box;
}

// Creates a GtkPicture from a file path, scaled to fit within max_width x max_height
static GtkWidget *make_image(const char *file_path, int max_width, int max_height) {
    GtkWidget *picture = gtk_picture_new_for_filename(file_path);
    gtk_picture_set_can_shrink(GTK_PICTURE(picture), TRUE);
    gtk_picture_set_content_fit(GTK_PICTURE(picture), GTK_CONTENT_FIT_CONTAIN);
    gtk_widget_set_size_request(picture, max_width, max_height);
    gtk_widget_set_halign(picture, GTK_ALIGN_CENTER);
    return picture;
}

// =============================================================================
//  Image Gallery — load, display, and remove images dynamically
// =============================================================================

// Updates the image count label
static void img_update_count() {
    std::string text = std::to_string(img_children.size()) + " image(s) in gallery";
    gtk_label_set_text(GTK_LABEL(img_count_label), text.c_str());
}

// Reuse the DOM slide-out pattern for image removal
struct ImgRemoveData {
    GtkWidget *card;
};

static void img_remove_value_cb(double value, gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    int offset = (int)(80.0 * (1.0 - value));
    gtk_widget_set_margin_start(widget, offset);
    gtk_widget_set_opacity(widget, value);
}

static void img_remove_done_cb(AdwAnimation * /*anim*/, gpointer user_data) {
    auto *data = static_cast<ImgRemoveData *>(user_data);
    GtkWidget *card = data->card;
    for (auto it = img_children.begin(); it != img_children.end(); ++it) {
        if (*it == card) {
            img_children.erase(it);
            break;
        }
    }
    gtk_box_remove(GTK_BOX(img_container), card);
    img_update_count();
    delete data;
}

// Delete a single image card
static void on_img_delete_clicked(GtkButton *button, gpointer user_data) {
    GtkWidget *card = GTK_WIDGET(user_data);
    animate_button_pop(GTK_WIDGET(button));

    AdwAnimationTarget *target = adw_callback_animation_target_new(
        img_remove_value_cb, card, nullptr);
    AdwAnimation *anim = adw_timed_animation_new(
        card, 1.0, 0.0, 300, target);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_IN_CUBIC);

    auto *remove_data = new ImgRemoveData{card};
    g_signal_connect(anim, "done", G_CALLBACK(img_remove_done_cb), remove_data);
    adw_animation_play(anim);
}

// Slide-in for newly added image
static void img_add_value_cb(double value, gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    int offset = (int)(60.0 * (1.0 - value));
    gtk_widget_set_margin_start(widget, offset);
    gtk_widget_set_opacity(widget, value);
}

// Builds an image card: [image + filename label + delete button] and appends it to the gallery
static void add_image_to_gallery(const char *file_path) {
    img_element_id++;

    // Outer card — vertical box: image on top, info row below
    GtkWidget *card = make_box(4);
    gtk_widget_set_margin_top(card, 4);
    gtk_widget_set_margin_bottom(card, 4);

    // The image itself
    GtkWidget *picture = make_image(file_path, 400, 250);
    gtk_box_append(GTK_BOX(card), picture);

    // Info row: [filename label  |  delete button]
    GtkWidget *info_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(card), info_row);

    // Extract just the filename from the path
    std::string fname = std::filesystem::path(file_path).filename().string();
    std::string markup = "<span weight='bold'>Image #" + std::to_string(img_element_id)
                         + "</span>  —  " + fname;
    GtkWidget *name_label = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(name_label), markup.c_str());
    gtk_label_set_xalign(GTK_LABEL(name_label), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(name_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_hexpand(name_label, TRUE);
    gtk_box_append(GTK_BOX(info_row), name_label);

    // Delete button for this image card
    GtkWidget *del_btn = gtk_button_new_with_label("Remove");
    gtk_widget_set_halign(del_btn, GTK_ALIGN_END);
    g_signal_connect(del_btn, "clicked", G_CALLBACK(on_img_delete_clicked), card);
    gtk_box_append(GTK_BOX(info_row), del_btn);

    // Separator under each card
    gtk_box_append(GTK_BOX(card), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL));

    // Append to container and track
    gtk_box_append(GTK_BOX(img_container), card);
    img_children.push_back(card);
    img_update_count();

    // Slide-in animation
    gtk_widget_set_opacity(card, 0.0);
    gtk_widget_set_margin_start(card, 60);
    AdwAnimationTarget *target = adw_callback_animation_target_new(
        img_add_value_cb, card, nullptr);
    AdwAnimation *anim = adw_timed_animation_new(
        card, 0.0, 1.0, 400, target);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_OUT_BACK);
    adw_animation_play(anim);
}

// File chooser response callback — called when user picks a file
static void on_file_chosen(GObject *source, GAsyncResult *result, gpointer /*user_data*/) {
    GtkFileDialog *dialog = GTK_FILE_DIALOG(source);
    GFile *file = gtk_file_dialog_open_finish(dialog, result, nullptr);
    if (file) {
        char *path = g_file_get_path(file);
        if (path) {
            add_image_to_gallery(path);
            g_free(path);
        }
        g_object_unref(file);
    }
}

// "Load Image" button — opens a native file picker filtered to images
static void on_load_image_clicked(GtkButton *button, gpointer /*user_data*/) {
    animate_button_pop(GTK_WIDGET(button));

    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Pick an image");

    // Set up image file filter
    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_filter_add_mime_type(filter, "image/gif");
    gtk_file_filter_add_mime_type(filter, "image/webp");
    gtk_file_filter_add_mime_type(filter, "image/svg+xml");
    gtk_file_filter_add_mime_type(filter, "image/bmp");
    gtk_file_filter_add_mime_type(filter, "image/tiff");

    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    gtk_file_dialog_set_default_filter(dialog, filter);

    gtk_file_dialog_open(dialog, GTK_WINDOW(main_window), nullptr, on_file_chosen, nullptr);

    g_object_unref(filter);
    g_object_unref(filters);
}

// "Clear Gallery" button — removes all images with staggered animation
static void on_clear_gallery_clicked(GtkButton *button, gpointer /*user_data*/) {
    animate_button_pop(GTK_WIDGET(button));

    std::vector<GtkWidget *> to_remove = img_children;
    for (size_t i = 0; i < to_remove.size(); i++) {
        GtkWidget *card = to_remove[i];

        AdwAnimationTarget *target = adw_callback_animation_target_new(
            img_remove_value_cb, card, nullptr);
        AdwAnimation *anim = adw_timed_animation_new(
            card, 1.0, 0.0, 250, target);
        adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_IN_CUBIC);

        auto *remove_data = new ImgRemoveData{card};
        g_signal_connect(anim, "done", G_CALLBACK(img_remove_done_cb), remove_data);

        g_timeout_add((guint)(i * 100),
            +[](gpointer data) -> gboolean {
                adw_animation_play(ADW_ANIMATION(data));
                return G_SOURCE_REMOVE;
            }, anim);
    }
}

// =============================================================================
//  activate() — builds the entire UI when the app launches
// =============================================================================
static void activate(GtkApplication *app, gpointer /*user_data*/) {
    section_widgets.clear();

    // ---- Main window ----
    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "Vamsi's C++ UI — Animated");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 650, 750);

    // ---- Scrollable wrapper ----
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_window_set_child(GTK_WINDOW(main_window), scrolled);

    // ---- Root vertical box ----
    GtkWidget *root_box = make_box(8);
    gtk_widget_set_margin_start(root_box, 16);
    gtk_widget_set_margin_end(root_box, 16);
    gtk_widget_set_margin_top(root_box, 16);
    gtk_widget_set_margin_bottom(root_box, 16);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), root_box);

    // =================================================================
    //  SECTION 1: Header  (with hover animation)
    // =================================================================
    GtkWidget *header_box = make_box(6);
    attach_hover_animation(header_box);
    gtk_box_append(GTK_BOX(root_box), header_box);
    section_widgets.push_back(header_box);

    gtk_box_append(GTK_BOX(header_box), make_heading("Hello from Vamsi's C++ App", 1));
    gtk_box_append(GTK_BOX(header_box), make_paragraph(
        "This entire UI is built from pure C++ using GTK4 + libadwaita. "
        "No HTML. No CSS. No JavaScript. No web server. "
        "Every widget is a native GTK object. Hover over sections to see "
        "them animate. Click buttons for pop effects. Watch sections slide in."
    ));

    gtk_box_append(GTK_BOX(root_box), make_separator());

    // =================================================================
    //  SECTION 2: About  (with hover animation)
    // =================================================================
    GtkWidget *about_box = make_box(6);
    attach_hover_animation(about_box);
    gtk_box_append(GTK_BOX(root_box), about_box);
    section_widgets.push_back(about_box);

    gtk_box_append(GTK_BOX(about_box), make_heading("About This Project", 2));
    gtk_box_append(GTK_BOX(about_box), make_paragraph(
        "This is a native desktop application written entirely in C++ "
        "using GTK4 and libadwaita. Every heading, paragraph, and button "
        "is a native widget — no markup languages involved."
    ));
    gtk_box_append(GTK_BOX(about_box), make_paragraph(
        "Animations are powered by AdwTimedAnimation with easing curves "
        "like EASE_OUT_CUBIC, EASE_OUT_BACK, and EASE_OUT_BOUNCE. "
        "Hover detection uses GtkEventControllerMotion."
    ));
    gtk_box_append(GTK_BOX(about_box), make_paragraph(
        "The hover effect animates margins and opacity to simulate "
        "a scale-up. The slide effect animates margin-top and opacity "
        "for a smooth reveal. Button pops use a sine-based bounce."
    ));

    gtk_box_append(GTK_BOX(root_box), make_separator());

    // =================================================================
    //  SECTION 3: Click Counter (with hover + button pop animation)
    // =================================================================
    GtkWidget *counter_box = make_box(8);
    attach_hover_animation(counter_box);
    gtk_box_append(GTK_BOX(root_box), counter_box);
    section_widgets.push_back(counter_box);

    gtk_box_append(GTK_BOX(counter_box), make_heading("Click Counter", 2));
    gtk_box_append(GTK_BOX(counter_box), make_paragraph(
        "Press the button below and watch the count go up. "
        "The button does a bounce-pop animation on each click."
    ));

    counter_label = make_paragraph("You have clicked: 0 times");
    gtk_box_append(GTK_BOX(counter_box), counter_label);

    GtkWidget *counter_btn = gtk_button_new_with_label("Click Me!");
    gtk_widget_set_halign(counter_btn, GTK_ALIGN_START);
    g_signal_connect(counter_btn, "clicked", G_CALLBACK(on_counter_clicked), nullptr);
    gtk_box_append(GTK_BOX(counter_box), counter_btn);

    gtk_box_append(GTK_BOX(root_box), make_separator());

    // =================================================================
    //  SECTION 4: Toggle Hidden Content (slide animation)
    // =================================================================
    GtkWidget *toggle_box = make_box(8);
    attach_hover_animation(toggle_box);
    gtk_box_append(GTK_BOX(root_box), toggle_box);
    section_widgets.push_back(toggle_box);

    gtk_box_append(GTK_BOX(toggle_box), make_heading("Toggle Hidden Content", 2));
    gtk_box_append(GTK_BOX(toggle_box), make_paragraph(
        "Click the button to slide-reveal or slide-hide the secret content. "
        "Uses AdwTimedAnimation with EASE_OUT_BACK for a springy entrance."
    ));

    GtkWidget *toggle_btn = gtk_button_new_with_label("Show / Hide");
    gtk_widget_set_halign(toggle_btn, GTK_ALIGN_START);
    g_signal_connect(toggle_btn, "clicked", G_CALLBACK(on_toggle_clicked), nullptr);
    gtk_box_append(GTK_BOX(toggle_box), toggle_btn);

    // Hidden content (starts invisible — will slide in/out)
    hidden_box_revealer = make_box(4);
    gtk_widget_set_visible(hidden_box_revealer, FALSE);
    gtk_widget_set_opacity(hidden_box_revealer, 0.0);
    gtk_box_append(GTK_BOX(toggle_box), hidden_box_revealer);

    gtk_box_append(GTK_BOX(hidden_box_revealer), make_heading("You found the hidden content!", 3));
    gtk_box_append(GTK_BOX(hidden_box_revealer), make_paragraph(
        "This paragraph slid into view using AdwTimedAnimation. "
        "The margin-top animates from 40px to 0 while opacity goes from "
        "0.0 to 1.0, creating a smooth slide-and-fade effect. "
        "Click the button again to slide it back out."
    ));

    gtk_box_append(GTK_BOX(root_box), make_separator());

    // =================================================================
    //  SECTION 5: DOM Playground (dynamic add/delete elements)
    // =================================================================
    GtkWidget *dom_box = make_box(8);
    attach_hover_animation(dom_box);
    gtk_box_append(GTK_BOX(root_box), dom_box);
    section_widgets.push_back(dom_box);

    gtk_box_append(GTK_BOX(dom_box), make_heading("DOM Playground", 2));
    gtk_box_append(GTK_BOX(dom_box), make_paragraph(
        "This mimics the DOM of web-based apps. Click 'Add Element' to "
        "dynamically create a new widget (like document.createElement + "
        "appendChild). Each element has its own 'Delete' button that "
        "removes just that element (like element.remove()). 'Clear All' "
        "wipes every child with a staggered animation."
    ));

    // Count label (like container.children.length)
    dom_count_label = make_paragraph("0 element(s) in container");
    gtk_box_append(GTK_BOX(dom_box), dom_count_label);

    // Buttons row: [Add Element]  [Clear All]
    GtkWidget *dom_btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(dom_box), dom_btn_row);

    GtkWidget *add_btn = gtk_button_new_with_label("Add Element");
    g_signal_connect(add_btn, "clicked", G_CALLBACK(on_dom_add_clicked), nullptr);
    gtk_box_append(GTK_BOX(dom_btn_row), add_btn);

    GtkWidget *clear_btn = gtk_button_new_with_label("Clear All");
    g_signal_connect(clear_btn, "clicked", G_CALLBACK(on_dom_clear_clicked), nullptr);
    gtk_box_append(GTK_BOX(dom_btn_row), clear_btn);

    // The container where dynamic children are appended (like a <div id="container">)
    dom_container = make_box(4);
    gtk_box_append(GTK_BOX(dom_box), dom_container);

    gtk_box_append(GTK_BOX(root_box), make_separator());

    // =================================================================
    //  SECTION 6: Image Gallery (load, display, remove images)
    // =================================================================
    GtkWidget *img_box = make_box(8);
    attach_hover_animation(img_box);
    gtk_box_append(GTK_BOX(root_box), img_box);
    section_widgets.push_back(img_box);

    gtk_box_append(GTK_BOX(img_box), make_heading("Image Gallery", 2));
    gtk_box_append(GTK_BOX(img_box), make_paragraph(
        "Render images entirely from C++ — like <img> in HTML but using "
        "GtkPicture. Click 'Load Image' to open a native file picker "
        "(like <input type=\"file\">) and choose any image from disk. "
        "Each image gets a 'Remove' button. 'Clear Gallery' wipes all. "
        "Sample images are pre-loaded below."
    ));

    // Count label
    img_count_label = make_paragraph("0 image(s) in gallery");
    gtk_box_append(GTK_BOX(img_box), img_count_label);

    // Buttons row: [Load Image]  [Clear Gallery]
    GtkWidget *img_btn_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_append(GTK_BOX(img_box), img_btn_row);

    GtkWidget *load_btn = gtk_button_new_with_label("Load Image");
    g_signal_connect(load_btn, "clicked", G_CALLBACK(on_load_image_clicked), nullptr);
    gtk_box_append(GTK_BOX(img_btn_row), load_btn);

    GtkWidget *clear_gallery_btn = gtk_button_new_with_label("Clear Gallery");
    g_signal_connect(clear_gallery_btn, "clicked", G_CALLBACK(on_clear_gallery_clicked), nullptr);
    gtk_box_append(GTK_BOX(img_btn_row), clear_gallery_btn);

    // Image container (like a <div> that holds <img> elements)
    img_container = make_box(8);
    gtk_box_append(GTK_BOX(img_box), img_container);

    // Pre-load sample images if they exist
    {
        // Get the directory where the executable lives
        std::filesystem::path exe_dir = std::filesystem::canonical("/proc/self/exe").parent_path();
        std::vector<std::string> samples = {"sample1.jpg", "sample2.jpg"};
        for (const auto &name : samples) {
            std::filesystem::path img_path = exe_dir / name;
            if (std::filesystem::exists(img_path)) {
                add_image_to_gallery(img_path.c_str());
            }
        }
    }

    gtk_box_append(GTK_BOX(root_box), make_separator());

    // =================================================================
    //  SECTION 7: Live Text Mirror (with hover)
    // =================================================================
    GtkWidget *mirror_box = make_box(8);
    attach_hover_animation(mirror_box);
    gtk_box_append(GTK_BOX(root_box), mirror_box);
    section_widgets.push_back(mirror_box);

    gtk_box_append(GTK_BOX(mirror_box), make_heading("Live Text Mirror", 2));
    gtk_box_append(GTK_BOX(mirror_box), make_paragraph(
        "Type something in the text field and the label below will "
        "mirror it in real time."
    ));

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Start typing here...");
    g_signal_connect(entry, "changed", G_CALLBACK(on_entry_changed), nullptr);
    gtk_box_append(GTK_BOX(mirror_box), entry);

    mirror_label = make_heading("(type something above)", 3);
    gtk_box_append(GTK_BOX(mirror_box), mirror_label);

    gtk_box_append(GTK_BOX(root_box), make_separator());

    // =================================================================
    //  SECTION 8: Reset Everything (with hover + button pop)
    // =================================================================
    GtkWidget *reset_box = make_box(8);
    attach_hover_animation(reset_box);
    gtk_box_append(GTK_BOX(root_box), reset_box);
    section_widgets.push_back(reset_box);

    gtk_box_append(GTK_BOX(reset_box), make_heading("Reset Everything", 2));
    gtk_box_append(GTK_BOX(reset_box), make_paragraph(
        "This button resets the counter, clears the text field, "
        "and slides the secret content back out."
    ));

    GtkWidget *reset_btn = gtk_button_new_with_label("Reset All");
    gtk_widget_set_halign(reset_btn, GTK_ALIGN_START);
    g_signal_connect(reset_btn, "clicked", G_CALLBACK(on_reset_clicked), entry);
    gtk_box_append(GTK_BOX(reset_box), reset_btn);

    gtk_box_append(GTK_BOX(root_box), make_separator());

    // =================================================================
    //  SECTION 9: Footer
    // =================================================================
    GtkWidget *footer_box = make_box(4);
    gtk_box_append(GTK_BOX(root_box), footer_box);
    section_widgets.push_back(footer_box);

    gtk_box_append(GTK_BOX(footer_box), make_paragraph(
        "Built with C++, GTK4, and libadwaita. "
        "Hover, slide, bounce animations, and image rendering — all from pure C++ code."
    ));

    // ---- Show the window ----
    gtk_window_present(GTK_WINDOW(main_window));

    // ---- Kick off staggered entrance animations ----
    schedule_entrance_animations();
}

//  main() — entry point
int main(int argc, char **argv) {
    AdwApplication *app = adw_application_new("com.vamsi.cppui", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}