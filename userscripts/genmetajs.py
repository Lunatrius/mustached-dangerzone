#!/usr/bin/env python
import re
import os
import logging

__ROOT__ = os.getcwd()
__REGEX__ = re.compile('(.*)\.user\.js')
__SCRIPTBEGIN__ = '// ==UserScript==\n'
__SCRIPTEND__ = '// ==/UserScript==\n'

for root, dirs, files in os.walk(__ROOT__):
    for f in files:
        try:
            results = re.findall(__REGEX__, f)
            if results and results[0]:
                result = results[0]
                userfile = os.path.join(root, f)
                metafile = os.path.join(root, results[0] + '.meta.js')

                print('[+] Processing', os.path.relpath(userfile, __ROOT__).replace('\\', '/'))
                with open(userfile, 'r') as userf:
                    contents = userf.read()
                    begin = contents.find(__SCRIPTBEGIN__)
                    end = contents.find(__SCRIPTEND__) + len(__SCRIPTEND__)

                    print('[ ] Writing', os.path.relpath(metafile, __ROOT__).replace('\\', '/'))
                    with open(metafile, 'w') as metaf:
                        metaf.write(contents[begin:end])
        except:
            logging.exception("Whoops...")
