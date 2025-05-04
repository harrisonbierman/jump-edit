# Jump Edit CLI tool

Jump Edit (je) allows users to change directories and open a default editor using custom labels.

## Installation

Clone from github

```bash
git clone https://github.com/harrisonbierman/jump-edit.git
cd jump-edit
```

#

Install compilation dependency (GNU Database Manager)

```bash
# Ubuntu
sudo apt install libgdbm-dev
```

#

Compile option 1: Makefile

```bash
make
```

Compile option 2: manual compilation

```bash
gcc -O3 -o jump_edit jump_edit.c -lgdbm
```

#

Install binary, create bash function, then source

```bash
sudo bash install.sh
source /etc/profile.d/jump_edit.sh
```

#

Remove repository

```bash
rm -r ../jump-edit
```

# Example usage

## Pick a default editor

```bash
je default-editor vim
```

## Add and use custom labels

### (je add) Option 1: explicitly providing a shell directory

This is useful when the file you are tying to edit is not a direct child of the root of the project directory

```bash
# Add label, editor path, and shell directory.
je add mylabel ~/my-project-root/src/my_project.c ~/my-project-root
#       │          path to open editor┘                     │
#       └label                            directory to cd to┘

# Open file with vim and cd shell to ~/my-project-root.
je mylabel

# Only cd to ~/my-project-root without opening editor.
je -j mylabel

# Only open file in editor without cd.
je -e mylabel
```

### (je add) Option 2: not proividing shell directory

This is useful if

1. The edit path is a directory.
2. File to edit is a direct child of the root project directory.
3. The shell directory is not relevent.

```bash
# Example 1: file as editor path.
je add bash ~/.bashrc

# opens .bashrc file in editor and cd to ~/
je bash

# Example 2: directory as editor path.
je add con ~/.config

# opens .config directory in editor and cd to ~/.config/
je con
```

## Important je options

```bash
# Only cd to shell directory without opening editor.
je -j mylabel

# Only open editor without cd to shell directory.
je -e mylabel
```

## Other commands

```bash
je list # lists out added labels
je rm  # removes label
je --help # help page
```
