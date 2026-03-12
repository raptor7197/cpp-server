# Pure C++ Desktop UI with GTK4

A native desktop application built **entirely in C++** using the GTK4 toolkit.

**No HTML. No CSS. No JavaScript. No web server.**

Every widget — headings, paragraphs, buttons, text fields — is a native GTK4 object created purely from C++ code.

## Features

- **Headings & Paragraphs** — Labels styled with Pango markup (bold, sized) act as headings; regular labels act as paragraphs
- **Divs** — `GtkBox` vertical containers group content into sections, just like `<div>` would in HTML
- **Click Counter** — A button that increments a counter and updates a label in real time
- **Toggle Visibility** — A button that shows/hides a secret section using `gtk_widget_set_visible()`
- **Live Text Mirror** — A text entry field that mirrors typed input into a heading label as you type
- **Reset All** — A single button that resets every interactive element back to its default state

## Build & Run

### Prerequisites

- A C++17 compiler (`g++` or `clang++`)
- GTK4 development headers (`gtk4-devel` on Fedora, `libgtk-4-dev` on Debian/Ubuntu)

### Compile

```
g++ main.cpp -o app $(pkg-config --cflags --libs gtk4) -std=c++17
```

### Run

```
./app
```

## How It Works

| Concept | Web Equivalent | C++ / GTK4 Equivalent |
|---|---|---|
| Container / div | `<div>` | `GtkBox` (vertical box) |
| Heading | `<h1>`, `<h2>`, `<h3>` | `GtkLabel` with Pango markup (`<span size='xx-large' weight='bold'>`) |
| Paragraph | `<p>` | `GtkLabel` with word wrapping |
| Button | `<button>` | `GtkButton` |
| Text input | `<input type="text">` | `GtkEntry` |
| Horizontal rule | `<hr>` | `GtkSeparator` |
| Click handler | `addEventListener` | `g_signal_connect()` with a C++ callback |
| Show/hide | `style.display` | `gtk_widget_set_visible()` |
| Scrollable page | `overflow: auto` | `GtkScrolledWindow` |

## Project Structure

```
cpp-ui/
├── main.cpp      # The entire application — UI + logic
├── httplib.h     # (Legacy) header-only HTTP library, no longer used
└── ReadMe.md     # This file
```

## License

Do whatever you want with it :)