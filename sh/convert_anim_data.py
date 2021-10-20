import sys
import json


if __name__ == '__main__':
    with open(sys.argv[1]) as f:
        arr = json.loads(f.read())
        newarr = []
        for item in arr:
            newchanges = []
            for change in item['changes']:
                newchange = [
                    change['x'],
                    change['y'],
                    change['x1'],
                    change['y1'],
                    change['x2'],
                    change['y2'],
                ]
                newchanges.append(newchange)

            newitem = [
                item['timestamp'],
                newchanges,
            ]
            newarr.append(newitem)
        newcontents = json.dumps(newarr)

    with open(sys.argv[1], 'w') as f:
        f.write(newcontents)
