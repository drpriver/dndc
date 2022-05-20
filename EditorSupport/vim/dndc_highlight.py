import pydndc
import vim

did_defs = False
disabled = False
def do_defs() -> None:
    global did_defs
    if did_defs: return
    did_defs = True
    vim.command('hi DndType ctermfg=15 guifg=#aaaaaa')
    vim.command('hi DndTitle ctermfg=12 guifg=Blue')
    vim.command('hi DndBullet ctermfg=7 guifg=#444444')
    vim.command('hi DndNumberedList ctermfg=7 guifg=#444444')
    vim.command('hi DndAttribute ctermfg=4')
    vim.command('hi link DndLink Type')
    vim.command("call prop_type_add('DndType', {'highlight': 'DndType'})")
    vim.command("call prop_type_add('DndTitle', {'highlight': 'DndTitle'})")
    vim.command("call prop_type_add('DndBullet', {'highlight': 'DndBullet'})")
    vim.command("call prop_type_add('DndNumberedList', {'highlight': 'DndNumberedList'})")
    vim.command("call prop_type_add('DndLink', {'highlight': 'DndLink'})")
    vim.command("call prop_type_add('DndAttribute', {'highlight': 'DndAttribute'})")
    vim.command("call prop_type_add('Normal', {'highlight': 'Normal'})")
    vim.command("call prop_type_add('String', {'highlight': 'String'})")
    vim.command("call prop_type_add('Comment', {'highlight': 'Comment'})")
    vim.command("call prop_type_add('Number', {'highlight': 'Number'})")
    vim.command("call prop_type_add('Function', {'highlight': 'Function'})")

def toggle() -> None:
    global disabled
    disabled = not disabled

def highlight_buffer(lo=None, hi=None) -> None:
    if disabled: return
    do_defs()
    import time
    t0 = time.time()
    begin = 0
    if hi is not None:
        begin = max(lo-100, 0)
        text = '\n'.join(vim.current.buffer[begin:hi+1])
    else:
        text = '\n'.join(vim.current.buffer)
    print('post_join', (time.time()-t0)*1000, 'ms')
    highlights = pydndc.analyze_syntax_for_highlight(text)
    print('post_higlights', (time.time()-t0)*1000, 'ms')
    mapping = {
            pydndc.SynType.ATTRIBUTE          : 'DndAttribute',
            pydndc.SynType.ATTRIBUTE_ARGUMENT : 'DndAttribute',
            pydndc.SynType.CLASS              : 'DndAttribute',
            pydndc.SynType.DIRECTIVE          : 'DndAttribute',
            pydndc.SynType.DOUBLE_COLON       : 'DndType',
            pydndc.SynType.HEADER             : 'DndTitle',
            pydndc.SynType.NODE_TYPE          : 'DndType',
            pydndc.SynType.RAW_STRING         : 'String',
            pydndc.SynType.JS_BRACE           : 'Function',
            pydndc.SynType.JS_BUILTIN         : 'Normal',
            pydndc.SynType.JS_COMMENT         : 'Comment',
            pydndc.SynType.JS_IDENTIFIER      : 'Normal',
            pydndc.SynType.JS_KEYWORD         : 'Function',
            pydndc.SynType.JS_KEYWORD_VALUE   : 'Normal',
            pydndc.SynType.JS_NODETYPE        : 'DndType',
            pydndc.SynType.JS_NUMBER          : 'Number',
            pydndc.SynType.JS_REGEX           : 'String',
            pydndc.SynType.JS_STRING          : 'String',
            pydndc.SynType.JS_VAR             : 'Normal',
    }
    for line_no, syns in highlights.items():
        line_no += 1
        line_no += begin
        if lo is not None and hi is not None:
            if not (lo <= line_no <= hi):
                continue
        vim.command(f"call prop_clear({line_no})")
        for syn in syns:
            type, col, _, length = syn
            col += 1
            tn = mapping[type]
            vim.command(f"call prop_add({line_no}, {col}, {{'length':{length}, 'type':'{tn}'}})")
    print('post prop_add', (time.time()-t0)*1000, 'ms')
