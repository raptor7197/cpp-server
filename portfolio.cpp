#include <adwaita.h>
#include <gtk/gtk.h>
#include <string>
#include <cstring>
#include <cmath>
#include <vector>

// =============================================================================
//  Vamsi Krishna — Portfolio Home Page
//  Pure C++ GTK4 + libadwaita replication of https://thekrishna.me/
//
//  No HTML. No CSS files. No JavaScript. No web server.
//  Dark theme with accent colors, hover animations, staggered entrance.
//
//  Compile:
//    g++ portfolio.cpp -o portfolio $(pkg-config --cflags --libs libadwaita-1) -std=c++17 && ./portfolio
// =============================================================================

// ---- Globals ----
static GtkWidget *main_window = nullptr;
static std::vector<GtkWidget *> section_widgets;

// ---- Section pointers for navbar scroll-to ----
static GtkWidget *scrolled_window = nullptr;
static GtkWidget *section_about = nullptr;
static GtkWidget *section_work = nullptr;
static GtkWidget *section_skills = nullptr;
static GtkWidget *section_projects = nullptr;
static GtkWidget *section_hero = nullptr;
static GtkWidget *section_contact = nullptr;

// Smooth-scroll animation value callback
static void scroll_anim_value_cb(double value, gpointer ud) {
    GtkWidget *s = GTK_WIDGET(ud);
    int from = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(s), "scroll-from"));
    int to = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(s), "scroll-to"));
    double v = from + (to - from) * value;
    GtkAdjustment *adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(s));
    gtk_adjustment_set_value(adj, v);
}

// Timeout callback that computes target position and starts the scroll animation
static gboolean scroll_timeout_cb(gpointer data) {
    GtkWidget *sw = GTK_WIDGET(data);
    GtkWidget *target_w = GTK_WIDGET(g_object_get_data(G_OBJECT(sw), "scroll-target"));
    if (!target_w) return G_SOURCE_REMOVE;

    // Compute the y position of the target widget
    graphene_point_t src_pt = GRAPHENE_POINT_INIT(0, 0);
    graphene_point_t dest_pt;
    if (!gtk_widget_compute_point(target_w,
            gtk_scrolled_window_get_child(GTK_SCROLLED_WINDOW(sw)),
            &src_pt, &dest_pt)) {
        return G_SOURCE_REMOVE;
    }

    GtkAdjustment *vadj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(sw));
    double current = gtk_adjustment_get_value(vadj);
    double target_val = (double)dest_pt.y - 20;
    if (target_val < 0) target_val = 0;

    g_object_set_data(G_OBJECT(sw), "scroll-from", GINT_TO_POINTER((int)current));
    g_object_set_data(G_OBJECT(sw), "scroll-to", GINT_TO_POINTER((int)target_val));

    AdwAnimationTarget *at = adw_callback_animation_target_new(scroll_anim_value_cb, sw, nullptr);
    AdwAnimation *anim = adw_timed_animation_new(sw, 0.0, 1.0, 600, at);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_OUT_CUBIC);
    adw_animation_play(anim);

    return G_SOURCE_REMOVE;
}

// Smooth-scroll to a widget inside the scrolled window
static void scroll_to_widget(GtkWidget *target) {
    if (!target || !scrolled_window) return;
    g_object_set_data(G_OBJECT(scrolled_window), "scroll-target", target);
    g_timeout_add(50, scroll_timeout_cb, scrolled_window);
}

// ---- Color constants (matching the portfolio dark theme) ----
// We use GTK CSS provider for the dark background + accent colors
static const char *APP_CSS = R"CSS(
    window, .main-bg {
        background-color: #0a0a0a;
        color: #e0e0e0;
    }
    .hero-name {
        font-size: 72px;
        font-weight: 800;
        color: #ffffff;
        letter-spacing: 6px;
    }
    .hero-sub {
        font-size: 72px;
        font-weight: 800;
        color: #ffffff;
        letter-spacing: 6px;
    }
    .hero-tagline {
        font-size: 16px;
        color: #888888;
    }
    .nav-btn {
        background: transparent;
        color: #888888;
        border: none;
        font-size: 14px;
        padding: 8px 16px;
        border-radius: 20px;
        min-height: 0;
    }
    .nav-btn:hover {
        color: #ffffff;
        background: rgba(255,255,255,0.08);
    }
    .accent-dot {
        color: #e94560;
    }
    .section-heading {
        font-size: 36px;
        font-weight: 700;
        color: #ffffff;
        letter-spacing: 2px;
    }
    .section-subheading {
        font-size: 14px;
        color: #666666;
        letter-spacing: 4px;
    }
    .about-text {
        font-size: 15px;
        color: #aaaaaa;
        line-height: 1.8;
    }
    .card-bg {
        background-color: #111111;
        border-radius: 16px;
        padding: 24px;
        border: 1px solid #1a1a1a;
    }
    .card-bg:hover {
        background-color: #161616;
        border-color: #2a2a2a;
    }
    .card-role {
        font-size: 18px;
        font-weight: 700;
        color: #ffffff;
    }
    .card-company {
        font-size: 14px;
        color: #e94560;
        font-weight: 600;
    }
    .card-period {
        font-size: 12px;
        color: #555555;
    }
    .card-desc {
        font-size: 13px;
        color: #888888;
    }
    .project-card {
        background-color: #111111;
        border-radius: 16px;
        padding: 20px;
        border: 1px solid #1a1a1a;
    }
    .project-card:hover {
        background-color: #161616;
        border-color: #e94560;
    }
    .project-title {
        font-size: 16px;
        font-weight: 700;
        color: #ffffff;
    }
    .project-category {
        font-size: 11px;
        color: #0a0a0a;
        background-color: #e94560;
        border-radius: 12px;
        padding: 2px 10px;
        font-weight: 600;
    }
    .project-url {
        font-size: 12px;
        color: #666666;
    }
    .skill-chip {
        background-color: #1a1a1a;
        color: #cccccc;
        border-radius: 20px;
        padding: 6px 16px;
        font-size: 13px;
        border: 1px solid #222222;
        min-height: 0;
    }
    .skill-chip:hover {
        background-color: #222222;
        color: #ffffff;
        border-color: #e94560;
    }
    .skill-category {
        font-size: 13px;
        font-weight: 600;
        color: #e94560;
        letter-spacing: 2px;
    }
    .status-dot {
        background-color: #2ecc71;
        border-radius: 50px;
        min-width: 8px;
        min-height: 8px;
        padding: 0;
        margin: 0;
    }
    .status-text {
        font-size: 13px;
        color: #2ecc71;
        font-weight: 500;
    }
    .contact-heading {
        font-size: 48px;
        font-weight: 800;
        color: #ffffff;
    }
    .contact-sub {
        font-size: 15px;
        color: #888888;
    }
    .contact-btn {
        background-color: #e94560;
        color: #ffffff;
        border-radius: 25px;
        padding: 12px 32px;
        font-size: 15px;
        font-weight: 600;
        border: none;
    }
    .contact-btn:hover {
        background-color: #ff6b81;
    }
    .link-btn {
        background: transparent;
        color: #666666;
        border: 1px solid #222222;
        border-radius: 20px;
        padding: 8px 20px;
        font-size: 13px;
        min-height: 0;
    }
    .link-btn:hover {
        color: #ffffff;
        border-color: #e94560;
    }
    .footer-text {
        font-size: 12px;
        color: #444444;
    }
    .separator-dark {
        background-color: #1a1a1a;
        min-height: 1px;
    }
    .navbar-bg {
        background-color: rgba(10, 10, 10, 0.9);
        border-bottom: 1px solid #1a1a1a;
    }
    .available-badge {
        background-color: rgba(46, 204, 113, 0.1);
        border-radius: 20px;
        padding: 4px 12px;
        border: 1px solid rgba(46, 204, 113, 0.3);
    }
)CSS";

// =============================================================================
//  Animation helpers (same pattern as main.cpp)
// =============================================================================

static void hover_anim_value_cb(double value, gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    int margin = (int)(8.0 * (1.0 - value));
    gtk_widget_set_margin_start(widget, margin);
    gtk_widget_set_margin_end(widget, margin);
    double opacity = 0.88 + 0.12 * value;
    gtk_widget_set_opacity(widget, opacity);
}

static void attach_hover_animation(GtkWidget *widget) {
    gtk_widget_set_margin_start(widget, 8);
    gtk_widget_set_margin_end(widget, 8);
    gtk_widget_set_opacity(widget, 0.88);

    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "enter",
        G_CALLBACK(+[](GtkEventControllerMotion*, double, double, gpointer ud) {
            GtkWidget *w = GTK_WIDGET(ud);
            AdwAnimationTarget *t = adw_callback_animation_target_new(hover_anim_value_cb, w, nullptr);
            AdwAnimation *a = adw_timed_animation_new(w, 0.0, 1.0, 200, t);
            adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(a), ADW_EASE_OUT_CUBIC);
            adw_animation_play(a);
        }), widget);
    g_signal_connect(motion, "leave",
        G_CALLBACK(+[](GtkEventControllerMotion*, gpointer ud) {
            GtkWidget *w = GTK_WIDGET(ud);
            AdwAnimationTarget *t = adw_callback_animation_target_new(hover_anim_value_cb, w, nullptr);
            AdwAnimation *a = adw_timed_animation_new(w, 1.0, 0.0, 300, t);
            adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(a), ADW_EASE_IN_OUT_CUBIC);
            adw_animation_play(a);
        }), widget);
    gtk_widget_add_controller(widget, motion);
}

static void entrance_value_cb(double value, gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    int offset = (int)(50.0 * (1.0 - value));
    gtk_widget_set_margin_top(widget, offset);
    gtk_widget_set_opacity(widget, value);
}

static gboolean entrance_start_one(gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    AdwAnimationTarget *target = adw_callback_animation_target_new(entrance_value_cb, widget, nullptr);
    AdwAnimation *anim = adw_timed_animation_new(widget, 0.0, 1.0, 600, target);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_OUT_CUBIC);
    adw_animation_play(anim);
    return G_SOURCE_REMOVE;
}

static void schedule_entrance_animations() {
    for (size_t i = 0; i < section_widgets.size(); i++) {
        GtkWidget *w = section_widgets[i];
        gtk_widget_set_opacity(w, 0.0);
        gtk_widget_set_margin_top(w, 50);
        g_timeout_add((guint)(150 + i * 130), entrance_start_one, w);
    }
}

static void button_pop_value_cb(double value, gpointer user_data) {
    GtkWidget *widget = GTK_WIDGET(user_data);
    double squeeze = sin(value * G_PI) * 3.0;
    gtk_widget_set_margin_top(widget, (int)squeeze);
}

static void animate_button_pop(GtkWidget *button) {
    AdwAnimationTarget *target = adw_callback_animation_target_new(button_pop_value_cb, button, nullptr);
    AdwAnimation *anim = adw_timed_animation_new(button, 0.0, 1.0, 200, target);
    adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(anim), ADW_EASE_OUT_BOUNCE);
    adw_animation_play(anim);
}

// =============================================================================
//  Widget factory helpers
// =============================================================================

static GtkWidget *make_label(const char *text, const char *css_class) {
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    if (css_class) gtk_widget_add_css_class(label, css_class);
    return label;
}

static GtkWidget *make_label_markup(const char *markup, const char *css_class) {
    GtkWidget *label = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    if (css_class) gtk_widget_add_css_class(label, css_class);
    return label;
}

static GtkWidget *make_label_center(const char *text, const char *css_class) {
    GtkWidget *label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.5f);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gtk_label_set_wrap(GTK_LABEL(label), TRUE);
    if (css_class) gtk_widget_add_css_class(label, css_class);
    return label;
}

static GtkWidget *make_vbox(int spacing) {
    return gtk_box_new(GTK_ORIENTATION_VERTICAL, spacing);
}

static GtkWidget *make_hbox(int spacing) {
    return gtk_box_new(GTK_ORIENTATION_HORIZONTAL, spacing);
}

static GtkWidget *make_separator() {
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_add_css_class(sep, "separator-dark");
    return sep;
}

// Open a URL in the default browser
static void open_url(const char *url) {
    GtkUriLauncher *launcher = gtk_uri_launcher_new(url);
    gtk_uri_launcher_launch(launcher, GTK_WINDOW(main_window), nullptr, nullptr, nullptr);
    g_object_unref(launcher);
}

// Gesture callback: opens URL stored in widget's "href" data
static void on_click_open_url(GtkGestureClick*, int, double, double, gpointer ud) {
    GtkWidget *w = GTK_WIDGET(ud);
    const char *u = (const char *)g_object_get_data(G_OBJECT(w), "href");
    if (u && strlen(u) > 0) open_url(u);
}

// Makes any widget clickable — opens a URL when clicked
static void make_clickable_url(GtkWidget *widget, const char *url) {
    g_object_set_data_full(G_OBJECT(widget), "href", g_strdup(url), g_free);
    gtk_widget_set_cursor_from_name(widget, "pointer");

    GtkGesture *click = gtk_gesture_click_new();
    g_signal_connect(click, "released", G_CALLBACK(on_click_open_url), widget);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click));
}

// Gesture callback: scrolls to section stored in widget's "scroll-dest" data
static void on_click_scroll_to(GtkGestureClick*, int, double, double, gpointer ud) {
    GtkWidget *w = GTK_WIDGET(ud);
    GtkWidget *dest = GTK_WIDGET(g_object_get_data(G_OBJECT(w), "scroll-dest"));
    if (dest) scroll_to_widget(dest);
}

// Makes any widget clickable — scrolls to a section when clicked
static void make_clickable_scroll(GtkWidget *widget, GtkWidget *target_section) {
    g_object_set_data(G_OBJECT(widget), "scroll-dest", target_section);
    gtk_widget_set_cursor_from_name(widget, "pointer");

    GtkGesture *click = gtk_gesture_click_new();
    g_signal_connect(click, "released", G_CALLBACK(on_click_scroll_to), widget);
    gtk_widget_add_controller(widget, GTK_EVENT_CONTROLLER(click));
}

// ---- Navbar callbacks (extracted from lambdas to avoid G_CALLBACK macro issues) ----
static void on_nav_clicked(GtkButton *b, gpointer) {
    animate_button_pop(GTK_WIDGET(b));
    int idx = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(b), "nav-idx"));
    GtkWidget *targets[] = {section_about, section_work, section_skills, section_projects, section_contact};
    if (idx >= 0 && idx < 5 && targets[idx])
        scroll_to_widget(targets[idx]);
}

static void on_resume_clicked(GtkButton *b, gpointer) {
    animate_button_pop(GTK_WIDGET(b));
    open_url("https://thekrishna.me/Vamsi%20Krishna%20Resume.pdf");
}

static void on_visit_site_clicked(GtkButton *b, gpointer) {
    animate_button_pop(GTK_WIDGET(b));
    open_url("https://thekrishna.me");
}

static void on_email_clicked(GtkButton *b, gpointer) {
    animate_button_pop(GTK_WIDGET(b));
    open_url("mailto:vamsikrishna.raptor@gmail.com");
}

static void on_github_clicked(GtkButton *b, gpointer) {
    animate_button_pop(GTK_WIDGET(b));
    open_url("https://github.com/raptor7197");
}

static void on_linkedin_clicked(GtkButton *b, gpointer) {
    animate_button_pop(GTK_WIDGET(b));
    open_url("https://linkedin.com/in/thekrishna");
}

static void on_back_to_top_clicked(GtkButton *b, gpointer) {
    animate_button_pop(GTK_WIDGET(b));
    if (section_hero) scroll_to_widget(section_hero);
}

static void on_view_project_clicked(GtkButton *b, gpointer) {
    animate_button_pop(GTK_WIDGET(b));
    const char *url = (const char *)g_object_get_data(G_OBJECT(b), "url");
    if (url && strlen(url) > 0) open_url(url);
}

// =============================================================================
//  Build the NAVBAR
// =============================================================================
static GtkWidget *build_navbar() {
    GtkWidget *bar = make_hbox(0);
    gtk_widget_add_css_class(bar, "navbar-bg");
    gtk_widget_set_margin_start(bar, 24);
    gtk_widget_set_margin_end(bar, 24);
    gtk_widget_set_size_request(bar, -1, 56);

    // Logo — click to scroll to top
    GtkWidget *logo = gtk_label_new(nullptr);
    gtk_label_set_markup(GTK_LABEL(logo), "<span weight='bold' size='large' color='#ffffff'>VK</span><span color='#e94560'>.</span>");
    gtk_widget_set_hexpand(logo, FALSE);
    gtk_box_append(GTK_BOX(bar), logo);

    // Spacer
    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(bar), spacer);

    // Nav links — each scrolls to its section
    const char *nav_items[] = {"About", "Work", "Skills", "Projects", "Contact", nullptr};
    for (int i = 0; nav_items[i]; i++) {
        GtkWidget *btn = gtk_button_new_with_label(nav_items[i]);
        gtk_widget_add_css_class(btn, "nav-btn");
        gtk_widget_set_valign(btn, GTK_ALIGN_CENTER);
        g_object_set_data(G_OBJECT(btn), "nav-idx", GINT_TO_POINTER(i));
        g_signal_connect(btn, "clicked", G_CALLBACK(on_nav_clicked), nullptr);
        gtk_box_append(GTK_BOX(bar), btn);
    }

    // Resume button — opens resume PDF URL
    GtkWidget *resume_btn = gtk_button_new_with_label("Resume");
    gtk_widget_add_css_class(resume_btn, "link-btn");
    gtk_widget_set_valign(resume_btn, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_start(resume_btn, 8);
    g_signal_connect(resume_btn, "clicked", G_CALLBACK(on_resume_clicked), nullptr);
    gtk_box_append(GTK_BOX(bar), resume_btn);

    return bar;
}

// =============================================================================
//  Build the HERO section
// =============================================================================
static GtkWidget *build_hero() {
    GtkWidget *hero = make_vbox(8);
    gtk_widget_set_halign(hero, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(hero, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(hero, 80);
    gtk_widget_set_margin_bottom(hero, 80);

    // "Available to work" badge
    GtkWidget *badge_box = make_hbox(6);
    gtk_widget_set_halign(badge_box, GTK_ALIGN_CENTER);
    gtk_widget_add_css_class(badge_box, "available-badge");

    GtkWidget *dot = gtk_drawing_area_new();
    gtk_widget_set_size_request(dot, 8, 8);
    gtk_widget_add_css_class(dot, "status-dot");
    gtk_widget_set_valign(dot, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(badge_box), dot);

    GtkWidget *status_lbl = gtk_label_new("Available to work");
    gtk_widget_add_css_class(status_lbl, "status-text");
    gtk_box_append(GTK_BOX(badge_box), status_lbl);

    gtk_box_append(GTK_BOX(hero), badge_box);

    // Spacer
    GtkWidget *sp1 = make_vbox(0);
    gtk_widget_set_size_request(sp1, -1, 20);
    gtk_box_append(GTK_BOX(hero), sp1);

    // CREATIVE
    GtkWidget *creative = make_label_center("CREATIVE", "hero-name");
    gtk_box_append(GTK_BOX(hero), creative);

    // DEVELOPER
    GtkWidget *developer = make_label_center("DEVELOPER", "hero-sub");
    gtk_box_append(GTK_BOX(hero), developer);

    // Spacer
    GtkWidget *sp2 = make_vbox(0);
    gtk_widget_set_size_request(sp2, -1, 16);
    gtk_box_append(GTK_BOX(hero), sp2);

    // Tagline
    GtkWidget *tagline = make_label_center(
        "Specializing in frontend development and digital experiences.",
        "hero-tagline"
    );
    gtk_box_append(GTK_BOX(hero), tagline);

    // Spacer
    GtkWidget *sp3 = make_vbox(0);
    gtk_widget_set_size_request(sp3, -1, 16);
    gtk_box_append(GTK_BOX(hero), sp3);

    // "Visit thekrishna.me" button — opens the live website
    GtkWidget *visit_btn = gtk_button_new_with_label("thekrishna.me →");
    gtk_widget_add_css_class(visit_btn, "link-btn");
    gtk_widget_set_halign(visit_btn, GTK_ALIGN_CENTER);
    g_signal_connect(visit_btn, "clicked", G_CALLBACK(on_visit_site_clicked), nullptr);
    gtk_box_append(GTK_BOX(hero), visit_btn);

    return hero;
}

// =============================================================================
//  Build the ABOUT section
// =============================================================================
static GtkWidget *build_about() {
    GtkWidget *section = make_vbox(16);
    gtk_widget_set_margin_start(section, 48);
    gtk_widget_set_margin_end(section, 48);

    // Section label
    GtkWidget *sub = make_label("// ABOUT", "section-subheading");
    gtk_box_append(GTK_BOX(section), sub);

    GtkWidget *heading = make_label("About Me", "section-heading");
    gtk_box_append(GTK_BOX(section), heading);

    GtkWidget *sp = make_vbox(0);
    gtk_widget_set_size_request(sp, -1, 8);
    gtk_box_append(GTK_BOX(section), sp);

    GtkWidget *p1 = make_label(
        "I currently work in the React ecosystem, leveraging tools like "
        "Next.js, Tailwind CSS, and Framer Motion to build performant, "
        "accessible, and visually compelling web applications.",
        "about-text"
    );
    gtk_box_append(GTK_BOX(section), p1);

    GtkWidget *p2 = make_label(
        "I'm passionate about creative development — blending code with design "
        "to craft unique digital experiences. When I'm not coding, you'll find me "
        "exploring new technologies, contributing to open source, or experimenting "
        "with Three.js and WebGL.",
        "about-text"
    );
    gtk_widget_set_margin_top(p2, 8);
    gtk_box_append(GTK_BOX(section), p2);

    return section;
}

// =============================================================================
//  Build a single WORK EXPERIENCE card
// =============================================================================
struct WorkEntry {
    const char *role;
    const char *company;
    const char *period;
    const char *description;
    const char *company_url;  // clickable link for the company
};

static GtkWidget *build_work_card(const WorkEntry &entry) {
    GtkWidget *card = make_vbox(8);
    gtk_widget_add_css_class(card, "card-bg");

    // Top row: role + period
    GtkWidget *top = make_hbox(0);
    gtk_box_append(GTK_BOX(card), top);

    GtkWidget *role = make_label(entry.role, "card-role");
    gtk_widget_set_hexpand(role, TRUE);
    gtk_box_append(GTK_BOX(top), role);

    GtkWidget *period = make_label(entry.period, "card-period");
    gtk_widget_set_halign(period, GTK_ALIGN_END);
    gtk_widget_set_valign(period, GTK_ALIGN_CENTER);
    gtk_box_append(GTK_BOX(top), period);

    // Company — clickable, opens company URL
    GtkWidget *company = make_label(entry.company, "card-company");
    if (entry.company_url && strlen(entry.company_url) > 0) {
        make_clickable_url(company, entry.company_url);
    }
    gtk_box_append(GTK_BOX(card), company);

    // Description
    GtkWidget *desc = make_label(entry.description, "card-desc");
    gtk_widget_set_margin_top(desc, 4);
    gtk_box_append(GTK_BOX(card), desc);

    // Attach hover animation to the card
    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "enter",
        G_CALLBACK(+[](GtkEventControllerMotion*, double, double, gpointer ud) {
            GtkWidget *w = GTK_WIDGET(ud);
            AdwAnimationTarget *t = adw_callback_animation_target_new(
                +[](double v, gpointer d) {
                    gtk_widget_set_opacity(GTK_WIDGET(d), 0.85 + 0.15 * v);
                    gtk_widget_set_margin_start(GTK_WIDGET(d), (int)(4.0 * (1.0 - v)));
                }, w, nullptr);
            AdwAnimation *a = adw_timed_animation_new(w, 0.0, 1.0, 200, t);
            adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(a), ADW_EASE_OUT_CUBIC);
            adw_animation_play(a);
        }), card);
    g_signal_connect(motion, "leave",
        G_CALLBACK(+[](GtkEventControllerMotion*, gpointer ud) {
            GtkWidget *w = GTK_WIDGET(ud);
            AdwAnimationTarget *t = adw_callback_animation_target_new(
                +[](double v, gpointer d) {
                    gtk_widget_set_opacity(GTK_WIDGET(d), 0.85 + 0.15 * v);
                    gtk_widget_set_margin_start(GTK_WIDGET(d), (int)(4.0 * (1.0 - v)));
                }, w, nullptr);
            AdwAnimation *a = adw_timed_animation_new(w, 1.0, 0.0, 300, t);
            adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(a), ADW_EASE_IN_OUT_CUBIC);
            adw_animation_play(a);
        }), card);
    gtk_widget_add_controller(card, motion);

    return card;
}

// =============================================================================
//  Build the WORK EXPERIENCE section
// =============================================================================
static GtkWidget *build_work_experience() {
    GtkWidget *section = make_vbox(16);
    gtk_widget_set_margin_start(section, 48);
    gtk_widget_set_margin_end(section, 48);

    GtkWidget *sub = make_label("// EXPERIENCE", "section-subheading");
    gtk_box_append(GTK_BOX(section), sub);

    GtkWidget *heading = make_label("Work Experience", "section-heading");
    gtk_box_append(GTK_BOX(section), heading);

    GtkWidget *sp = make_vbox(0);
    gtk_widget_set_size_request(sp, -1, 8);
    gtk_box_append(GTK_BOX(section), sp);

    std::vector<WorkEntry> entries = {
        {
            "Research Intern",
            "Aegion Dynamic Solutions",
            "Jan 2026 - Present",
            "Working on building a Custom Domain Specific language for "
            "restricted internal use cases using ANTLR and Go, aiming to "
            "improve efficiency and security.",
            "https://aegionds.com"
        },
        {
            "Projects Lead",
            "IEEE VIT Vellore",
            "May 2024 - Present",
            "Leading technical projects and mentoring junior members. "
            "Rose through the ranks from Junior Core to Senior Core "
            "and now Projects Lead.",
            "https://ieeevit.org"
        },
        {
            "Software Developer Intern",
            "AIAT India",
            "Jul 2025 - Nov 2025",
            "Worked as a Full Stack Developer intern contributing to various "
            "web application projects. Developed responsive web applications "
            "using React.js and Tailwind CSS.",
            "https://aiatindia.com"
        },
        {
            "Associate Member",
            "The Quantumplators",
            "Feb 2025 - Sep 2025",
            "Researched Quantum Computing concepts and contributed to "
            "community knowledge sharing.",
            ""
        },
        {
            "Web & Creatives Volunteer",
            "International Test Conference 2025",
            "Feb 2025 - Jul 2025",
            "Volunteered for the Web and Creatives team at the International "
            "Test Conference India. Created visual assets and creatives for "
            "event promotion.",
            "https://itcindia.org"
        },
    };

    for (auto &e : entries) {
        GtkWidget *card = build_work_card(e);
        gtk_box_append(GTK_BOX(section), card);
    }

    return section;
}

// =============================================================================
//  Build a single PROJECT card
// =============================================================================
struct ProjectEntry {
    const char *title;
    const char *category;
    const char *url;
};

static GtkWidget *build_project_card(const ProjectEntry &entry) {
    GtkWidget *card = make_vbox(10);
    gtk_widget_add_css_class(card, "project-card");

    // Make the ENTIRE card clickable — opens the project URL
    make_clickable_url(card, entry.url);

    // Top row: category badge
    GtkWidget *cat_label = gtk_label_new(entry.category);
    gtk_widget_add_css_class(cat_label, "project-category");
    gtk_widget_set_halign(cat_label, GTK_ALIGN_START);
    gtk_box_append(GTK_BOX(card), cat_label);

    // Title
    GtkWidget *title = make_label(entry.title, "project-title");
    gtk_box_append(GTK_BOX(card), title);

    // URL preview label so users can see where it leads
    std::string short_url = entry.url;
    // Strip "https://" prefix for display
    if (short_url.rfind("https://", 0) == 0) short_url = short_url.substr(8);
    GtkWidget *url_label = make_label(short_url.c_str(), "project-url");
    gtk_widget_set_margin_top(url_label, 2);
    gtk_box_append(GTK_BOX(card), url_label);

    // Bottom row: View Project link
    GtkWidget *bottom = make_hbox(0);
    gtk_box_append(GTK_BOX(card), bottom);

    GtkWidget *spacer = gtk_label_new("");
    gtk_widget_set_hexpand(spacer, TRUE);
    gtk_box_append(GTK_BOX(bottom), spacer);

    GtkWidget *view_btn = gtk_button_new_with_label("View Project →");
    gtk_widget_add_css_class(view_btn, "link-btn");
    gtk_widget_set_halign(view_btn, GTK_ALIGN_END);

    // Store URL as object data so we can open it on click
    g_object_set_data_full(G_OBJECT(view_btn), "url", g_strdup(entry.url), g_free);
    g_signal_connect(view_btn, "clicked", G_CALLBACK(on_view_project_clicked), nullptr);
    gtk_box_append(GTK_BOX(bottom), view_btn);

    // Hover animation for the whole card
    GtkEventController *motion = gtk_event_controller_motion_new();
    g_signal_connect(motion, "enter",
        G_CALLBACK(+[](GtkEventControllerMotion*, double, double, gpointer ud) {
            GtkWidget *w = GTK_WIDGET(ud);
            AdwAnimationTarget *t = adw_callback_animation_target_new(
                +[](double v, gpointer d) {
                    gtk_widget_set_opacity(GTK_WIDGET(d), 0.85 + 0.15 * v);
                    int m = (int)(6.0 * (1.0 - v));
                    gtk_widget_set_margin_start(GTK_WIDGET(d), m);
                    gtk_widget_set_margin_end(GTK_WIDGET(d), m);
                }, w, nullptr);
            AdwAnimation *a = adw_timed_animation_new(w, 0.0, 1.0, 200, t);
            adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(a), ADW_EASE_OUT_CUBIC);
            adw_animation_play(a);
        }), card);
    g_signal_connect(motion, "leave",
        G_CALLBACK(+[](GtkEventControllerMotion*, gpointer ud) {
            GtkWidget *w = GTK_WIDGET(ud);
            AdwAnimationTarget *t = adw_callback_animation_target_new(
                +[](double v, gpointer d) {
                    gtk_widget_set_opacity(GTK_WIDGET(d), 0.85 + 0.15 * v);
                    int m = (int)(6.0 * (1.0 - v));
                    gtk_widget_set_margin_start(GTK_WIDGET(d), m);
                    gtk_widget_set_margin_end(GTK_WIDGET(d), m);
                }, w, nullptr);
            AdwAnimation *a = adw_timed_animation_new(w, 1.0, 0.0, 300, t);
            adw_timed_animation_set_easing(ADW_TIMED_ANIMATION(a), ADW_EASE_IN_OUT_CUBIC);
            adw_animation_play(a);
        }), card);
    gtk_widget_add_controller(card, motion);

    return card;
}

// =============================================================================
//  Build the SELECTED WORK / PROJECTS section
// =============================================================================
static GtkWidget *build_projects() {
    GtkWidget *section = make_vbox(16);
    gtk_widget_set_margin_start(section, 48);
    gtk_widget_set_margin_end(section, 48);

    GtkWidget *sub = make_label("// PROJECTS", "section-subheading");
    gtk_box_append(GTK_BOX(section), sub);

    GtkWidget *heading = make_label("Selected Work", "section-heading");
    gtk_box_append(GTK_BOX(section), heading);

    GtkWidget *sp = make_vbox(0);
    gtk_widget_set_size_request(sp, -1, 8);
    gtk_box_append(GTK_BOX(section), sp);

    std::vector<ProjectEntry> projects = {
        {"LLM Chat Interface",               "LLM",           "https://github.com/raptor7197/agentic-knowledge-base"},
        {"My Devfolio",                       "Frontend",      "https://github.com/raptor7197/god-knows"},
        {"Scrape Krunch - Web Scraping Tool", "LLM",           "https://github.com/raptor7197/scrape-krunch"},
        {"Kisan Mitra - Multilingual Farming Assistant", "Frontend", "https://github.com/raptor7197/KisanMitra"},
        {"Quantum Signers",                   "CyberSecurity", "https://github.com/raptor7197/signed-with-quantum"},
        {"Shell Forge - Unix Shell in C++",   "Miscellaneous", "https://github.com/raptor7197/shell-forge"},
        {"Eco-friendly E-commerce Platform",  "Full Stack",    "https://github.com/raptor7197/GreenCart"},
        {"Hybrid Encryption System",          "CyberSecurity", "https://github.com/raptor7197/Hybrid-Encryption"},
    };

    // 2-column grid using a GtkGrid
    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_column_homogeneous(GTK_GRID(grid), TRUE);

    for (size_t i = 0; i < projects.size(); i++) {
        GtkWidget *card = build_project_card(projects[i]);
        gtk_grid_attach(GTK_GRID(grid), card, (int)(i % 2), (int)(i / 2), 1, 1);
    }
    gtk_box_append(GTK_BOX(section), grid);

    return section;
}

// =============================================================================
//  Build the SKILLS section
// =============================================================================
struct SkillCategory {
    const char *name;
    std::vector<const char *> items;
};

static GtkWidget *build_skills() {
    GtkWidget *section = make_vbox(16);
    gtk_widget_set_margin_start(section, 48);
    gtk_widget_set_margin_end(section, 48);

    GtkWidget *sub = make_label("// SKILLS", "section-subheading");
    gtk_box_append(GTK_BOX(section), sub);

    GtkWidget *heading = make_label("My Skills", "section-heading");
    gtk_box_append(GTK_BOX(section), heading);

    GtkWidget *sp = make_vbox(0);
    gtk_widget_set_size_request(sp, -1, 8);
    gtk_box_append(GTK_BOX(section), sp);

    std::vector<SkillCategory> categories = {
        {"LANGUAGES", {"JavaScript", "TypeScript", "Python", "C++", "C", "Java", "Go", "SQL", "HTML", "CSS"}},
        {"LIBRARIES & FRAMEWORKS", {"React", "Next.js", "Node.js", "Express", "Tailwind CSS", "Framer Motion", "Three.js", "GSAP", "Flask", "FastAPI"}},
        {"TOOLS & SOFTWARE", {"Git", "GitHub", "VS Code", "Docker", "AWS", "Firebase", "MongoDB", "PostgreSQL", "Figma", "Linux"}},
        {"OTHER", {"ANTLR", "WebGL", "REST APIs", "GraphQL", "Webpack", "Vite", "Vercel", "Netlify", "Postman", "Jira"}},
    };

    for (auto &cat : categories) {
        GtkWidget *cat_box = make_vbox(10);
        gtk_widget_set_margin_top(cat_box, 12);

        GtkWidget *cat_label = make_label(cat.name, "skill-category");
        gtk_box_append(GTK_BOX(cat_box), cat_label);

        // Flow-like layout using a GtkFlowBox
        GtkWidget *flow = gtk_flow_box_new();
        gtk_flow_box_set_selection_mode(GTK_FLOW_BOX(flow), GTK_SELECTION_NONE);
        gtk_flow_box_set_max_children_per_line(GTK_FLOW_BOX(flow), 10);
        gtk_flow_box_set_column_spacing(GTK_FLOW_BOX(flow), 8);
        gtk_flow_box_set_row_spacing(GTK_FLOW_BOX(flow), 8);
        gtk_flow_box_set_homogeneous(GTK_FLOW_BOX(flow), FALSE);

        for (auto &skill : cat.items) {
            GtkWidget *chip = gtk_label_new(skill);
            gtk_widget_add_css_class(chip, "skill-chip");
            gtk_flow_box_insert(GTK_FLOW_BOX(flow), chip, -1);
        }

        gtk_box_append(GTK_BOX(cat_box), flow);
        gtk_box_append(GTK_BOX(section), cat_box);
    }

    return section;
}

// =============================================================================
//  Build the CONTACT section
// =============================================================================
static GtkWidget *build_contact() {
    GtkWidget *section = make_vbox(16);
    gtk_widget_set_margin_start(section, 48);
    gtk_widget_set_margin_end(section, 48);
    gtk_widget_set_halign(section, GTK_ALIGN_CENTER);

    GtkWidget *sub = make_label_center("// CONTACT", "section-subheading");
    gtk_box_append(GTK_BOX(section), sub);

    GtkWidget *heading = make_label_center("Let's Connect", "contact-heading");
    gtk_box_append(GTK_BOX(section), heading);

    GtkWidget *sp = make_vbox(0);
    gtk_widget_set_size_request(sp, -1, 8);
    gtk_box_append(GTK_BOX(section), sp);

    GtkWidget *desc = make_label_center(
        "Have a project in mind or just want to chat?\n"
        "Feel free to reach out — I'm always open to interesting conversations\n"
        "and new opportunities.",
        "contact-sub"
    );
    gtk_box_append(GTK_BOX(section), desc);

    GtkWidget *sp2 = make_vbox(0);
    gtk_widget_set_size_request(sp2, -1, 16);
    gtk_box_append(GTK_BOX(section), sp2);

    // Buttons row
    GtkWidget *btn_row = make_hbox(12);
    gtk_widget_set_halign(btn_row, GTK_ALIGN_CENTER);

    // Email button
    GtkWidget *email_btn = gtk_button_new_with_label("Say Hello ☕");
    gtk_widget_add_css_class(email_btn, "contact-btn");
    g_signal_connect(email_btn, "clicked", G_CALLBACK(on_email_clicked), nullptr);
    gtk_box_append(GTK_BOX(btn_row), email_btn);

    // GitHub
    GtkWidget *gh_btn = gtk_button_new_with_label("GitHub");
    gtk_widget_add_css_class(gh_btn, "link-btn");
    g_signal_connect(gh_btn, "clicked", G_CALLBACK(on_github_clicked), nullptr);
    gtk_box_append(GTK_BOX(btn_row), gh_btn);

    // LinkedIn
    GtkWidget *li_btn = gtk_button_new_with_label("LinkedIn");
    gtk_widget_add_css_class(li_btn, "link-btn");
    g_signal_connect(li_btn, "clicked", G_CALLBACK(on_linkedin_clicked), nullptr);
    gtk_box_append(GTK_BOX(btn_row), li_btn);

    gtk_box_append(GTK_BOX(section), btn_row);

    return section;
}

// =============================================================================
//  Build FOOTER
// =============================================================================
static GtkWidget *build_footer() {
    GtkWidget *footer = make_vbox(8);
    gtk_widget_set_margin_top(footer, 16);
    gtk_widget_set_margin_bottom(footer, 24);

    GtkWidget *line = make_label_center("Designed & Built by Vamsi Krishna", "footer-text");
    gtk_box_append(GTK_BOX(footer), line);

    GtkWidget *line2 = make_label_center("Built with C++ & GTK4 — No HTML, No CSS files, No JavaScript", "footer-text");
    gtk_box_append(GTK_BOX(footer), line2);

    // Footer links row
    GtkWidget *footer_links = make_hbox(16);
    gtk_widget_set_halign(footer_links, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(footer_links, 8);

    // Website link
    GtkWidget *web_btn = gtk_button_new_with_label("thekrishna.me");
    gtk_widget_add_css_class(web_btn, "link-btn");
    g_signal_connect(web_btn, "clicked", G_CALLBACK(on_visit_site_clicked), nullptr);
    gtk_box_append(GTK_BOX(footer_links), web_btn);

    // GitHub link
    GtkWidget *gh_btn = gtk_button_new_with_label("GitHub");
    gtk_widget_add_css_class(gh_btn, "link-btn");
    g_signal_connect(gh_btn, "clicked", G_CALLBACK(on_github_clicked), nullptr);
    gtk_box_append(GTK_BOX(footer_links), gh_btn);

    // LinkedIn link
    GtkWidget *li_btn = gtk_button_new_with_label("LinkedIn");
    gtk_widget_add_css_class(li_btn, "link-btn");
    g_signal_connect(li_btn, "clicked", G_CALLBACK(on_linkedin_clicked), nullptr);
    gtk_box_append(GTK_BOX(footer_links), li_btn);

    // Back to top
    GtkWidget *top_btn = gtk_button_new_with_label("↑ Back to top");
    gtk_widget_add_css_class(top_btn, "link-btn");
    g_signal_connect(top_btn, "clicked", G_CALLBACK(on_back_to_top_clicked), nullptr);
    gtk_box_append(GTK_BOX(footer_links), top_btn);

    gtk_box_append(GTK_BOX(footer), footer_links);

    return footer;
}

// =============================================================================
//  TITLE BAR ANIMATION — mimics the scrolling title from the original site
// =============================================================================

struct TitleAnimData {
    GtkWindow *window;
    std::string message;
    int pos;
    bool forward;
};

static TitleAnimData *title_data = nullptr;

static gboolean title_tick(gpointer /*ud*/) {
    if (!title_data) return G_SOURCE_REMOVE;

    int len = (int)title_data->message.length();

    if (title_data->forward) {
        if (title_data->pos < len) {
            title_data->pos++;
            std::string sub = title_data->message.substr(0, title_data->pos);
            gtk_window_set_title(title_data->window, sub.c_str());
        } else {
            title_data->forward = false;
        }
    } else {
        if (title_data->pos > 0) {
            title_data->pos--;
            int start = len - title_data->pos;
            std::string sub = title_data->message.substr(start, title_data->pos);
            gtk_window_set_title(title_data->window, sub.c_str());
        } else {
            title_data->forward = true;
        }
    }
    return G_SOURCE_CONTINUE;
}

// =============================================================================
//  activate() — builds the entire portfolio UI
// =============================================================================
static void activate(GtkApplication *app, gpointer /*user_data*/) {
    section_widgets.clear();

    // ---- Load CSS ----
    GtkCssProvider *css_provider = gtk_css_provider_new();
    gtk_css_provider_load_from_string(css_provider, APP_CSS);
    gtk_style_context_add_provider_for_display(
        gdk_display_get_default(),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );

    // ---- Main window ----
    main_window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(main_window), "Vamsi Krishna");
    gtk_window_set_default_size(GTK_WINDOW(main_window), 900, 800);
    gtk_widget_add_css_class(main_window, "main-bg");

    // ---- Title bar scrolling animation ----
    title_data = new TitleAnimData{GTK_WINDOW(main_window), "This is Vamsi Krishna", 0, true};
    g_timeout_add(200, title_tick, nullptr);

    // ---- Scrollable wrapper ----
    scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_window_set_child(GTK_WINDOW(main_window), scrolled_window);

    // ---- Root box ----
    GtkWidget *root = make_vbox(0);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), root);

    // ===================== HERO =====================
    GtkWidget *hero = build_hero();
    section_hero = hero;
    gtk_box_append(GTK_BOX(root), hero);
    section_widgets.push_back(hero);

    gtk_box_append(GTK_BOX(root), make_separator());

    // Spacer
    GtkWidget *s1 = make_vbox(0);
    gtk_widget_set_size_request(s1, -1, 48);
    gtk_box_append(GTK_BOX(root), s1);

    // ===================== ABOUT =====================
    GtkWidget *about = build_about();
    section_about = about;
    attach_hover_animation(about);
    gtk_box_append(GTK_BOX(root), about);
    section_widgets.push_back(about);

    GtkWidget *s2 = make_vbox(0);
    gtk_widget_set_size_request(s2, -1, 48);
    gtk_box_append(GTK_BOX(root), s2);
    gtk_box_append(GTK_BOX(root), make_separator());
    GtkWidget *s3 = make_vbox(0);
    gtk_widget_set_size_request(s3, -1, 48);
    gtk_box_append(GTK_BOX(root), s3);

    // ===================== WORK EXPERIENCE =====================
    GtkWidget *work = build_work_experience();
    section_work = work;
    attach_hover_animation(work);
    gtk_box_append(GTK_BOX(root), work);
    section_widgets.push_back(work);

    GtkWidget *s4 = make_vbox(0);
    gtk_widget_set_size_request(s4, -1, 48);
    gtk_box_append(GTK_BOX(root), s4);
    gtk_box_append(GTK_BOX(root), make_separator());
    GtkWidget *s5 = make_vbox(0);
    gtk_widget_set_size_request(s5, -1, 48);
    gtk_box_append(GTK_BOX(root), s5);

    // ===================== SKILLS =====================
    GtkWidget *skills = build_skills();
    section_skills = skills;
    attach_hover_animation(skills);
    gtk_box_append(GTK_BOX(root), skills);
    section_widgets.push_back(skills);

    GtkWidget *s6 = make_vbox(0);
    gtk_widget_set_size_request(s6, -1, 48);
    gtk_box_append(GTK_BOX(root), s6);
    gtk_box_append(GTK_BOX(root), make_separator());
    GtkWidget *s7 = make_vbox(0);
    gtk_widget_set_size_request(s7, -1, 48);
    gtk_box_append(GTK_BOX(root), s7);

    // ===================== PROJECTS =====================
    GtkWidget *projects = build_projects();
    section_projects = projects;
    attach_hover_animation(projects);
    gtk_box_append(GTK_BOX(root), projects);
    section_widgets.push_back(projects);

    GtkWidget *s8 = make_vbox(0);
    gtk_widget_set_size_request(s8, -1, 48);
    gtk_box_append(GTK_BOX(root), s8);
    gtk_box_append(GTK_BOX(root), make_separator());
    GtkWidget *s9 = make_vbox(0);
    gtk_widget_set_size_request(s9, -1, 48);
    gtk_box_append(GTK_BOX(root), s9);

    // ===================== CONTACT =====================
    GtkWidget *contact = build_contact();
    section_contact = contact;
    gtk_box_append(GTK_BOX(root), contact);
    section_widgets.push_back(contact);

    // ===================== NAVBAR (after sections exist) =====================
    // Build navbar AFTER sections so scroll targets are set
    GtkWidget *navbar = build_navbar();

    // Make logo scroll to top
    GtkWidget *logo_area = gtk_widget_get_first_child(navbar);
    if (logo_area) make_clickable_scroll(logo_area, section_hero);

    // Insert navbar at the top of root
    gtk_box_prepend(GTK_BOX(root), navbar);

    GtkWidget *s10 = make_vbox(0);
    gtk_widget_set_size_request(s10, -1, 32);
    gtk_box_append(GTK_BOX(root), s10);
    gtk_box_append(GTK_BOX(root), make_separator());

    // ===================== FOOTER =====================
    GtkWidget *footer = build_footer();
    gtk_box_append(GTK_BOX(root), footer);

    // ---- Present and animate ----
    gtk_window_present(GTK_WINDOW(main_window));
    schedule_entrance_animations();
}

// =============================================================================
//  main()
// =============================================================================
int main(int argc, char **argv) {
    AdwApplication *app = adw_application_new("com.vamsi.portfolio", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    if (title_data) { delete title_data; title_data = nullptr; }
    g_object_unref(app);
    return status;
}