import hexchat

__module_name__ = 'MissOpenEye Commands'
__module_author__ = 'Lunatrius'
__module_version__ = '0.2'
__module_description__ = 'MissOpenEye Commands'


# delay in seconds between multiple messages
delay = 1


def cmd_openeye(word, word_eol, userdata):
    if hexchat.get_info('channel') != '#OpenEye':
        print 'You are not in #OpenEye!'
        return hexchat.EAT_ALL

    if word[0].lower() == 'crashnote':
        note = word[len(word) - 1]
        print 'Sending %d messages...' % (len(word) - 2)

        for i in xrange(1, len(word) - 1):
            h = word[i]
            if len(h) == 32:
                msg = '#crash:note:set %s "%s"' % (h, note)
                cmd = 'timer %f say %s' % (((i - 1) * delay) + 0.1, msg)
                hexchat.command(cmd)
            else:
                print 'Invalid hash length (%d; %s)!' % (len(h), h)

    return hexchat.EAT_ALL


hexchat.hook_command('CRASHNOTE', cmd_openeye)
hexchat.prnt('%s version %s loaded.' % (__module_name__, __module_version__))
