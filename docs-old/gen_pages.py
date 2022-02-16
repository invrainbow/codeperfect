import os


def dedent(s):
    lines = s.splitlines()
    if not lines[0]:
        lines = lines[1:]
    if not lines[-1]:
        lines = lines[:-1]

    indent_size = len(lines[0]) - len(lines[0].lstrip())
    return '\n'.join(it[indent_size:] for it in lines)


def slugcase(x):
    return '-'.join(x.lower().split())


if __name__ == '__main__':
    menulines = []

    with open("pages.txt") as f:
        sections = f.read().split('\n\n')

    for i, section in enumerate(sections):
        lines = section.strip().splitlines()
        category, items = lines[0], lines[1:]

        category_slug = slugcase(category)
        try:
            os.mkdir(f'content/{category_slug}')
        except Exception as e:
            pass

        with open(f'content/{category_slug}/_index.md', 'w') as f:
            f.write(dedent(f'''
                ---
                title : "{category}"
                ---
            '''))

        for j, item in enumerate(items):
            item_slug = slugcase(item)

            with open(f'content/{category_slug}/{item_slug}.md', 'w') as f:
                f.write(dedent(f'''
                    ---
                    title: "{item}"
                    menu:
                      docs:
                        parent: "{category_slug}"
                    weight: {(j+1)*10}
                    toc: true
                    ---
                '''))

        menulines.append(dedent(f'''
            [[docs]]
              name = "{category}"
              weight = {(i+1)*10}
              identifier = "{category_slug}"
              url = "/{category_slug}"
        '''))

    with open('config/_default/menus.toml', 'w') as f:
        f.write('\n\n'.join(menulines))
