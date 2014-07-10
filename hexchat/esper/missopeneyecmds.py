import hexchat

__module_name__ = 'MissOpenEye Commands'
__module_author__ = 'Lunatrius'
__module_version__ = '0.3.1'
__module_description__ = 'MissOpenEye Commands'


# delay in seconds between multiple messages
delay = 1


def cmd_openeye(word, word_eol, userdata):
    if hexchat.get_info('channel') != '#OpenEye':
        hexchat.prnt('You are not in #OpenEye!')
        return hexchat.EAT_ALL

    cmd = word[0].upper()

    errs = []
    cmds = []

    if cmd == 'CRASHNOTESET':
        note = word[len(word) - 1]

        for h in set(word[1:-1]):
            if len(h) == 32:
                cmds.append('say #crash:note:set %s "%s"' % (h, note))
            else:
                errs.append('Invalid hash length (%d; %s)!' % (len(h), h))

    if cmd == 'CRASHNOTEUPDATE':
        note = word[len(word) - 1]

        for h in set(word[1:-1]):
            if len(h) == 32:
                cmds.append('say #crash:note:remove %s' % (h))
                cmds.append('say #crash:note:set %s "%s"' % (h, note))
            else:
                errs.append('Invalid hash length (%d; %s)!' % (len(h), h))

    if cmd == 'CRASHNOTEREMOVE':
        for h in set(word[1:]):
            if len(h) == 32:
                cmds.append('say #crash:note:remove %s' % (h))
            else:
                errs.append('Invalid hash length (%d; %s)!' % (len(h), h))

    if errs:
        hexchat.prnt('\n'.join(errs))

    if cmds:
        hexchat.prnt('Sending %d command(s)...' % (len(cmds)))
        for i in range(0, len(cmds)):
            cmd = 'timer %.1f %s' % (i * delay + 0.1, cmds[i])
            hexchat.command(cmd)

    return hexchat.EAT_ALL


hexchat.hook_command('CRASHNOTESET', cmd_openeye)
hexchat.hook_command('CRASHNOTEUPDATE', cmd_openeye)
hexchat.hook_command('CRASHNOTEREMOVE', cmd_openeye)
hexchat.prnt('%s version %s loaded.' % (__module_name__, __module_version__))
