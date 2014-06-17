import hexchat

__module_name__ = 'MissOpenEye NoSpam'
__module_author__ = 'Lunatrius'
__module_version__ = '0.1'
__module_description__ = 'Do not print crash/files messages from MissOpenEye.'


whitelist = [
    'lunatrius',
    'dynimc',
    'ingameinfo',
    'msh',
    'profiles',
    'schematica',
    'stackie',
]

whitelist = [thing.lower() for thing in whitelist]


def print_cb(word, word_eol, userdata, attrs):
    sender = word[0]
    msg = word[1].lower()

    if sender != 'MissOpenEye' or hexchat.get_info('channel') != '#OpenEye':
        return hexchat.EAT_NONE

    for thing in whitelist:
        if msg.find(thing) != -1:
            return hexchat.EAT_NONE

    if msg.find('\x0304\x02\x1fcrash') != -1 or msg.find('\x0303\x02\x1ffiles') != -1:
        return hexchat.EAT_ALL

    return hexchat.EAT_NONE


hexchat.hook_print_attrs('Channel Message', print_cb)
hexchat.prnt('%s version %s loaded.' % (__module_name__, __module_version__))
