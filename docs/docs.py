import os


commands = '''
Overview
Getting Started

Building
Setting up
Start a build

Debugging
Setting up
Start a debug session
Breakpoints
Examine program state
Step through program
Debug a test

Editor
Vim keybindings

Code Intelligence
Automatic completion
Jump to definition
Parameter hints
Postfix completion

Navigation
Go to file
Go to symbol

Refactoring
Rename a declaration
Rename a package

Commands
Commands

Productivity
Keyboard shortcuts
'''


def snakecase(x):
    return '-'.join(x.lower().split())


menulines = []
category_weight = 10
sections = commands.split('\n\n')
for section in sections:
    lines = section.strip().splitlines()
    category, items = lines[0], lines[1:]

    category_slug = snakecase(category)
    try:
        os.mkdir(f'content/{category_slug}')
    except Exception as e:
        pass

    with open(f'content/{category_slug}/_index.md', 'w') as f:
        f.write(f'''---
title : "{category}"
---
''')

    weight = 10
    for item in items:
        item_slug = snakecase(item)

        with open(f'content/{category_slug}/{item_slug}.md', 'w') as f:
            f.write(f'''---
title: "{item}"
menu:
  docs:
    parent: "{category_slug}"
weight: {weight}
toc: true
---
''')

        weight += 10

    menulines.append(f'''[[docs]]
name = "{category}"
weight = {category_weight}
identifier = "{category_slug}"
url = "/{category_slug}"''')

    category_weight += 10


with open('config/_default/menus.toml', 'w') as f:
    f.write('\n\n'.join(menulines))

