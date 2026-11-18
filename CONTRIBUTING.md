# Contributing to Brasero

Thank you for your interest in contributing to Brasero!

## Building

The preferred way to build and run Brasero for development is with [GNOME Builder](https://apps.gnome.org/Builder/):

1. Clone the repository and open the project folder in GNOME Builder.
2. Builder will automatically detect [`org.gnome.Brasero.Devel.json`](org.gnome.Brasero.Devel.json), set up the Flatpak SDK and dependencies, and build the project.
3. Click **Run** (or press `Ctrl+Shift+Space`) to build and launch Brasero.

Alternatively, you can build directly with [Meson](https://mesonbuild.com/) and Ninja (see [README.md](README.md#requirements) for required and optional dependencies):

```bash
meson setup _build
meson compile -C _build
meson test -C _build --print-errorlogs
```

To install locally (e.g. into `/usr/local` or a custom prefix passed via `--prefix`):

```bash
meson install -C _build
```

## Code Style

Brasero uses [Uncrustify](https://github.com/uncrustify/uncrustify) together with the `data/lineup-parameters` helper script to enforce a consistent C coding style (based on GNOME/Epiphany conventions). The exact configuration is defined in [`data/uncrustify.cfg`](data/uncrustify.cfg).

### Formatting Rules

- **Indentation & Whitespace**
  - Use **2 spaces** per indentation level; do not use tabs.
  - Indent `case` labels 1 level (2 spaces) inside `switch` statements, with `case` block braces unindented relative to the `case`.
  - Indent ternary operators (`? :`) by 2 levels (4 spaces).
  - Use Unix `LF` line endings and UTF-8 encoding without a BOM.
  - Ensure files end with a single trailing newline and have no leading blank lines.
  - Do not leave blank lines immediately after `{` or immediately before `}`.
  - Remove extra semicolons.

- **Braces**
  - Use **K&R style** (opening `{` on the same line) for control-flow statements (`if`, `else`, `else if`, `for`, `while`, `do`, `switch`, `case`), `struct`/`union`/`enum` definitions, and assignments.
  - Place the opening `{` on a **new line** for function definitions.
  - Single-line functions, enums, and assignments may remain on one line.

- **Spacing**
  - Put a space before `(` in function calls, prototypes, definitions, `return`, and `sizeof` (e.g., `foo (bar)`, `return (0);`, `sizeof (int)`).
  - **Exception:** Do not put a space before `(` for gettext translation macros (`_()`, `N_()`, `C_()`).
  - Put spaces around assignment, arithmetic, boolean, and comparison operators, and after commas.
  - Do not put spaces inside parentheses, between nested parentheses (`((` / `))`), after type casts (`(int)a`), or before semicolons.
  - Attach pointer `*` to the variable/type on the right: space before `*`, no space after `*` or between `**` (e.g., `char *str`, `GError **error`), except when followed by a qualifier (`const char * const`).

- **Function Declarations & Definitions**
  - Place the return type on its own line above the function name.
  - In multi-line parameter lists, put one parameter per line, with the first parameter on the same line as `(` and the closing `)` on the same line as the last parameter.
  - Align parameter names (and pointer `*` stars) using `data/lineup-parameters`, for example:
    ```c
    gboolean
    frobnitz (Frobnitz  *frobnitz,
              gint       magic_number,
              GError   **error)
    {
      ...
    }
    ```

- **Comments**
  - Use C-style `/* ... */` comments (`//` comments are converted to `/* ... */`).
  - Use leading `* ` on continuation lines in multi-line comments, with the closing `*/` on its own line.

### Checking Code Style Locally

The CI pipeline runs `data/check-code-style` on modified `.c` files. Before submitting a merge request, you can run the check locally (requires `uncrustify`, `python3`, and `diffutils`):

```bash
# Check .c files changed relative to origin/master (or HEAD~1)
./data/check-code-style

# Or check specific files
./data/check-code-style src/brasero-app.c
```

If any formatting differences are found, the script prints a diff and writes a patch file to `/tmp/brasero-ccs-*.patch` that you can apply with:

```bash
git apply /tmp/brasero-ccs-*.patch
```

## Use of Generative AI

This project does not allow contributions generated entirely by large languages models (LLMs) and chatbots. This limit includes, but is not limited to, tools like ChatGPT, Claude, Copilot, DeepSeek, and Devin AI. We are taking these steps as precaution due to the potential negative influence of AI generated content on quality, as well as likely copyright violations.

Contributions submitted that appear to have heavy AI assistance will require a full workup that demonstrates the submitter understands the content being submitted. These will be approved/denied on a case-by-case basis by the maintainers.

This limit of AI generated content applies to all parts of the projects, including, but not limited to, code, documentation, issues, and artworks. An exception applies for purely translating texts for issues and comments to English.

AI tools can be used to answer questions and find information. However, we encourage contributors to avoid them in favor of using [existing documentation](https://developer.gnome.org) and our [chats and forums](https://welcome.gnome.org). Since AI generated information is frequently misleading or false, we cannot supply support on anything referencing AI output.
