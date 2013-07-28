import re
import hexchat

__module_name__ = 'Multiline'
__module_author__ = 'Lunatrius'
__module_version__ = '0.1'
__module_description__ = 'Warns you about newlines in the input field'


def key_press(word, word_eol, userdata):
    global confirm
    key = int(word[0])
    modifier = int(word[1])

    if key == 65293 or key == 65421:
        message = hexchat.get_info('inputbox')

        if not re.search('[\r\n]', message):
            return hexchat.EAT_NONE

        if modifier == 0:
            if confirm:
                confirm = False
                return hexchat.EAT_NONE

            hexchat.prnt('The input field contains a multiline message. Press \002enter\002 again to send or \002shift+enter\002 to replace newlines with " | ".')
            confirm = True
            return hexchat.EAT_ALL

        elif modifier == 1:
            message = re.sub('(^\s+|\s+$)', '', message)
            message = re.sub('\s*[\r\n]+\s*', ' | ', message)
            hexchat.command('SETTEXT %s' % message)
            return hexchat.EAT_ALL

    else:
        confirm = False

    return hexchat.EAT_NONE

confirm = False

hexchat.hook_print('Key Press', key_press)
hexchat.prnt('%s version %s loaded.' % (__module_name__, __module_version__))
