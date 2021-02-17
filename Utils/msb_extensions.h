#ifndef MSB_EXTENSIONS_H
#define MSB_EXTENSIONS_H

static inline
int
msb_write_kebab(Nonnull(MStringBuilder*)msb, const Allocator a, Nonnull(const char*)text, size_t length){
    int n_written = 0;
    bool want_write_hyphen = false;
    for(size_t i = 0; i < length; i++){
        char c = text[i];
        switch(c){
            case 'A' ... 'Z':
                c |= 0x20; // tolower
                // fall-through
            case 'a' ... 'z':
            case '0' ... '9':
                if(want_write_hyphen){
                    msb_write_char(msb, a, '-');
                    want_write_hyphen = false;
                    }
                msb_write_char(msb, a, c);
                n_written += 1;
                continue;
            case ' ': case '\t': case '-':
                if(n_written)
                    want_write_hyphen = true;
                continue;
            default:
                continue;
            }
        }
    return n_written;
    }

static inline
void
msb_write_title(Nonnull(MStringBuilder*) restrict msb, const Allocator a, Nonnull(const char*) restrict str, size_t len){
    if(not len)
        return;
    _check_msb_size(msb, a, len);
    bool wants_cap = true;
    for(size_t i = 0; i < len; i++){
        char c = str[i];
        switch(c){
            case 'a' ... 'z':
                if(wants_cap){
                    c &= ~0x20;
                    wants_cap = false;
                    }
                break;
            case 'A' ... 'Z':
                wants_cap = false;
                break;
            default:
                c = ' ';
                wants_cap = true;
                break;
            }
        msb->data[msb->cursor++] = c;
        }
    }

static inline
void
msb_write_json_escaped_str(Nonnull(MStringBuilder*)restrict sb, const Allocator a, Nonnull(const char*)restrict str, size_t length){
    _check_msb_size(sb, a, length*2);
    auto data = sb->data;
    auto cursor = sb->cursor;
    for(size_t i = 0; i < length; i++){
        switch(str[i]){
            case '"':
                data[cursor++] = '\\';
                data[cursor++] = '"';
                break;
            case '\\':
                data[cursor++] = '\\';
                data[cursor++] = '\\';
                break;
            case '\b':
                data[cursor++] = '\\';
                data[cursor++] = 'b';
                break;
            case '\f':
                data[cursor++] = '\\';
                data[cursor++] = 'f';
                break;
            case '\n':
                data[cursor++] = '\\';
                data[cursor++] = 'n';
                break;
            case '\r':
                data[cursor++] = '\\';
                data[cursor++] = 'r';
                break;
            case '\t':
                data[cursor++] = '\\';
                data[cursor++] = 't';
                break;
            default:
                data[cursor++] = str[i];
                break;
            }
        }
    sb->cursor = cursor;
    }
#endif
