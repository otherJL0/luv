project = 'Luv'
description = 'Bare libuv bindings for lua'
merge = true
add_language_extension('lc', 'c')
file = {
    'src/luv.c', 'src/loop.c', 'src/req.c', 'src/handle.c', 'src/timer.c',
    'src/prepare.c', 'src/check.c', 'src/idle.c', 'src/async.c', 'src/poll.c',
    'src/signal.c', 'src/process.c', 'src/stream.c', 'src/tcp.c', 'src/pipe.c',
    'src/tty.c', 'src/udp.c', 'src/fs_event.c', 'src/fs_poll.c', 'src/fs.c',
    'src/dns.c', 'src/misc.c', 'src/thread.c', 'src/work.c', 'src/util.c',
    'src/metrics.c'
}
dir = 'docs'
readme = 'README.md'
format = 'markdown'
boilerplate = true
style = '!fixed'
custom_display_name_handler = function(item, default_handler)
    if item.type == 'function' then
        if string.find(item.kind, 'class') then
            return item.name .. '[class]'
        end
    end
    return default_handler(item)
end
