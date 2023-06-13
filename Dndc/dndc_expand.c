//
// Copyright © 2021-2023, David Priver <david@davidpriver.com>
//
#ifndef DNDC_EXPAND_C
#define DNDC_EXPAND_C
#include "dndc.h"
#include "dndc_types.h"
#include "dndc_funcs.h"
#include "dndc_logging.h"
#include "Utils/MStringBuilder.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
warn_unused
int
expand_node_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth);

static
warn_unused
int
expand_node(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth);

static
warn_unused
int
expand_md_list(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth);

static
warn_unused
int
expand_md_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth);

static
warn_unused
int
expand_table_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth);

static
warn_unused
int
expand_keyvalue_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth);

static
warn_unused
int
expand_to_dnd(DndcContext*ctx, MStringBuilder* msb){
    int result = 0;
    Node* node = get_node(ctx, ctx->root_handle);
    if(node->type != NODE_MD){
        result = expand_node(ctx, node, 0, msb, 0);
        // return DNDC_ERROR_INVALID_TREE;
    }
    else
        result = expand_node_body(ctx, node, 0, msb, 0);
    return result;
}

static
void
write_generic_header(DndcContext* ctx, Node* n, int indent, MStringBuilder*msb){
    msb_write_nchar(msb, ' ', indent);
    if(n->header.length)
        msb_write_str(msb, n->header.text, n->header.length);
    const StringView* hd = &NODETYPE_TO_NODE_ALIASES[n->type];
    MSB_FORMAT(msb, "::", *hd);
    if(n->flags & NODEFLAG_ID){
        // #uggh
        // This is super gross, I hate this.
        // FIXME: use handles in the expand instead of raw nodes.
        NodeHandle handle = {._value = (uint32_t)(n - ctx->nodes.data)};
        StringView id = node_get_id(ctx, handle);
        MSB_FORMAT(msb, " #id(", id, ")");
    }
    if(n->flags & NODEFLAG_HIDE){
        msb_write_literal(msb, " #hide");
    }
    if(n->flags & NODEFLAG_NOID){
        msb_write_literal(msb, " #noid");
    }
    if(n->flags & NODEFLAG_NOINLINE){
        msb_write_literal(msb, " #noinline");
    }
    RARRAY_FOR_EACH(StringView, cls, n->classes){
        MSB_FORMAT(msb, " .", *cls);
    }
    if(n->attributes){
        StringView2* items = AttrTable_items(n->attributes);
        size_t count = n->attributes->count;
        for(size_t i = 0; i < count; i++){
            StringView2* at = items + i;
            if(!at->key.length) continue;
            MSB_FORMAT(msb, " @", at->key);
            if(at->value.length){
                MSB_FORMAT(msb, "(", at->value, ")");
            }
        }
    }
    msb_write_char(msb, '\n');
}

static
int
expand_node(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth){
    if(node_depth > DNDC_MAX_NODE_DEPTH){
        NODE_LOG_ERROR(ctx, n, "Max node depth exceeded:", node_depth);
        return DNDC_ERROR_INVALID_TREE;
    }
    int result = 0;
    switch(n->type){
        case NODE_STRING:
            msb_write_nchar(msb, ' ', indent);
            if(n->header.length)
                msb_write_str(msb, n->header.text, n->header.length);
            msb_write_char(msb, '\n');
            return result;
        case NODE_STYLESHEETS:
        case NODE_SCRIPTS:
            write_generic_header(ctx, n, indent, msb);
            return expand_node_body(ctx, n, indent+2, msb, node_depth+1);
        case NODE_TITLE:
        case NODE_HEADING:
            write_generic_header(ctx, n, indent, msb);
            if(node_children_count(n)){
                NODE_LOG_ERROR(ctx, n, "TITLE or HEADING has children");
                return DNDC_ERROR_INVALID_TREE;
            }
            return result;
        case NODE_KEYVALUEPAIR:
        case NODE_LIST_ITEM:
        case NODE_LIST:
        case NODE_JS:
        case NODE_BULLETS:
        case NODE_TABLE_ROW:
        case NODE_PARA:
            NODE_LOG_ERROR(ctx,n, "Node escaped to top level: ", quoted(LS_to_SV(NODENAMES[n->type])));
            return DNDC_ERROR_INVALID_TREE;
        case NODE_META:
        case NODE_DEFLIST:
        case NODE_DEF:
        case NODE_DETAILS:
        case NODE_MD:
        case NODE_DIV:
        case NODE_TABLE:
        case NODE_LINKS:
        case NODE_IMAGE:
        case NODE_RAW:
        case NODE_PRE:
        case NODE_KEYVALUE:
        case NODE_IMGLINKS:
        case NODE_COMMENT:
        case NODE_QUOTE:
            write_generic_header(ctx, n, indent, msb);
            return expand_node_body(ctx, n, indent+2, msb, node_depth+1);
        case NODE_TOC:
            write_generic_header(ctx, n, indent, msb);
            if(node_children_count(n)){
                NODE_LOG_ERROR(ctx,n, "TOC has children");
                return DNDC_ERROR_INVALID_TREE;
            }
            return result;
        case NODE_IMPORT:
        case NODE_CONTAINER:{
            NODE_CHILDREN_FOR_EACH(child, n){
                result = expand_node(ctx, get_node(ctx, *child), indent, msb, node_depth+1);
                if(result) return result;
            }
            return result;
        }break;
        case NODE_INVALID:
            NODE_LOG_ERROR(ctx, n, "INVALID NODE");
            return DNDC_ERROR_INVALID_TREE;
    }
    unreachable();
}

static
int
expand_node_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth){
    int result = 0;
    switch(n->type){
        case NODE_DEFLIST:
        case NODE_DEF:
        case NODE_DETAILS:
        case NODE_MD:
            result = expand_md_body(ctx, n, indent, msb, node_depth);
            return result;
        case NODE_STYLESHEETS:
        case NODE_SCRIPTS:
        case NODE_QUOTE:
        case NODE_COMMENT:
        case NODE_IMGLINKS:
        case NODE_PRE:
        case NODE_RAW:
        case NODE_IMAGE:
        case NODE_LINKS:
        case NODE_DIV:
        case NODE_META:
            NODE_CHILDREN_FOR_EACH(ch, n){
                Node* child = get_node(ctx, *ch);
                result = expand_node(ctx, child, indent, msb, node_depth+1);
                if(result) return result;
            }
            return result;
        case NODE_TABLE:
            return expand_table_body(ctx, n, indent, msb, node_depth);
        case NODE_KEYVALUE:
            return expand_keyvalue_body(ctx, n, indent, msb, node_depth);
        case NODE_INVALID:
        case NODE_CONTAINER:
        case NODE_TOC:
        case NODE_KEYVALUEPAIR:
        case NODE_LIST_ITEM:
        case NODE_LIST:
        case NODE_BULLETS:
        case NODE_JS:
        case NODE_IMPORT:
        case NODE_TABLE_ROW:
        case NODE_HEADING:
        case NODE_TITLE:
        case NODE_PARA:
        case NODE_STRING:
            NODE_LOG_ERROR(ctx, n, "Node can't be expanded into text format: ", quoted(LS_to_SV(NODENAMES[n->type])));
            return DNDC_ERROR_INVALID_TREE;
    }
    unreachable();
}
static
int
expand_md_list(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth);
static
int
expand_md_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth);
static
int
expand_md_bullets(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int depth, int node_depth){
    if(node_depth > DNDC_MAX_NODE_DEPTH){
        NODE_LOG_ERROR(ctx, n, "Max node depth exceeded: ", node_depth);
        return DNDC_ERROR_INVALID_TREE;
    }
    int result = 0;
    NODE_CHILDREN_FOR_EACH(l, n){
        Node* li = get_node(ctx, *l);
        if(li->type != NODE_LIST_ITEM){
            NODE_LOG_ERROR(ctx, li, "Non list-item child of bullets: ", quoted(LS_to_SV(NODENAMES[li->type])));
            return DNDC_ERROR_INVALID_TREE;
        }
        msb_write_nchar(msb, ' ', indent);
        switch(depth){
            case 0:  msb_write_literal(msb, "* "); break;
            case 1:  msb_write_literal(msb, "+ "); break;
            default: msb_write_literal(msb, "- "); break;
        }
        size_t count = node_children_count(li);
        if(!count){
            NODE_LOG_ERROR(ctx, li, "List item must have at least one child.");
            return DNDC_ERROR_INVALID_TREE;
        }
        size_t i = 0;
        NODE_CHILDREN_FOR_EACH(subitem, li){
            Node* sub = get_node(ctx, *subitem);
            if(i == 0){
                if(sub->type != NODE_STRING){
                    NODE_LOG_ERROR(ctx, sub, "First list item must be a string, got ", quoted(LS_to_SV(NODENAMES[sub->type])));
                    return DNDC_ERROR_INVALID_TREE;
                }
                msb_write_str(msb, sub->header.text, sub->header.length);
                msb_write_char(msb, '\n');
                i++;
                continue;
            }
            if(sub->type == NODE_STRING){
                msb_write_nchar(msb, ' ', indent+2);
                msb_write_str(msb, sub->header.text, sub->header.length);
                msb_write_char(msb, '\n');
            }
            else if(sub->type == NODE_BULLETS){
                result = expand_md_bullets(ctx, sub, indent+2, msb, depth+1, node_depth+1);
                if(result) return result;
            }
            else if(sub->type == NODE_LIST){
                result = expand_md_list(ctx, sub, indent+2, msb, node_depth+1);
                if(result) return result;
            }
            else {
                NODE_LOG_ERROR(ctx, sub, "List items must contain strings, bullets or lists, got: ", quoted(LS_to_SV(NODENAMES[sub->type])));
                return DNDC_ERROR_INVALID_TREE;
            }
            i++;
        }
    }
    return result;
}
static
int
expand_md_list(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth){
    if(node_depth > DNDC_MAX_NODE_DEPTH){
        NODE_LOG_ERROR(ctx, n, "Max node depth exceeded: ", node_depth);
        return DNDC_ERROR_INVALID_TREE;
    }
    int result = 0;
    size_t list_number = 0;
    NODE_CHILDREN_FOR_EACH(l, n){
        list_number++;
        Node* li = get_node(ctx, *l);
        if(li->type != NODE_LIST_ITEM){
            NODE_LOG_ERROR(ctx, li, "Non list-item child of list: ", quotedls(NODENAMES[li->type]));
            return DNDC_ERROR_INVALID_TREE;
        }
        msb_write_nchar(msb, ' ', indent);
        MSB_FORMAT(msb, list_number, ". ");
        size_t count = node_children_count(li);
        if(!count){
            NODE_LOG_ERROR(ctx, li, "List Item must have at least one child");
            return DNDC_ERROR_INVALID_TREE;
        }
        size_t i = 0;
        NODE_CHILDREN_FOR_EACH(subitem, li){
            Node* sub = get_node(ctx, *subitem);
            if(i == 0){
                if(sub->type != NODE_STRING){
                    NODE_LOG_ERROR(ctx, sub, "First List Item must be a string.", quotedls(NODENAMES[sub->type]));
                    return DNDC_ERROR_INVALID_TREE;
                }
                msb_write_str(msb, sub->header.text, sub->header.length);
                msb_write_char(msb, '\n');
                i++;
                continue;
            }
            if(sub->type == NODE_STRING){
                msb_write_nchar(msb, ' ', indent+2);
                msb_write_str(msb, sub->header.text, sub->header.length);
                msb_write_char(msb, '\n');
            }
            else if(sub->type == NODE_BULLETS){
                result = expand_md_bullets(ctx, sub, indent+2, msb, 0, node_depth+1);
                if(result) return result;
            }
            else if(sub->type == NODE_LIST){
                result = expand_md_list(ctx, sub, indent+2, msb, node_depth+1);
                if(result) return result;
            }
            else {
                NODE_LOG_ERROR(ctx, sub, "List items must contain strings, bullets, or lists. Got: ", quotedls(NODENAMES[sub->type]));
                return DNDC_ERROR_INVALID_TREE;
            }
            i++;
        }
    }
    return result;

}
static
int
expand_md_para(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth){
    if(node_depth > DNDC_MAX_NODE_DEPTH){
        NODE_LOG_ERROR(ctx, n, "Max node depth exceeded: ", node_depth);
        return DNDC_ERROR_INVALID_TREE;
    }
    int result = 0;
    NODE_CHILDREN_FOR_EACH(c, n){
        Node* child = get_node(ctx, *c);
        if(child->type != NODE_STRING){
            NODE_LOG_ERROR(ctx, child, "Expected string node in md para, got: ", quotedls(NODENAMES[child->type]));
            return DNDC_ERROR_INVALID_TREE;
        }
        msb_write_nchar(msb, ' ', indent);
        MSB_FORMAT(msb, child->header, "\n");
    }
    msb_write_char(msb, '\n');
    return result;
}

static
int
expand_md_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth){
    if(node_depth > DNDC_MAX_NODE_DEPTH){
        NODE_LOG_ERROR(ctx, n, "Max node depth exceeded: ", node_depth);
        return DNDC_ERROR_INVALID_TREE;
    }
    int result = 0;
    NODE_CHILDREN_FOR_EACH(c, n){
        Node* child = get_node(ctx, *c);
        switch(child->type){
            case NODE_BULLETS:
                result = expand_md_bullets(ctx, child, indent, msb, 0, node_depth+1);
                if(result) return result;
                break;
            case NODE_PARA:
                result = expand_md_para(ctx, child, indent, msb, node_depth+1);
                if(result) return result;
                break;
            case NODE_LIST:
                result = expand_md_list(ctx, child, indent, msb, node_depth+1);
                if(result) return result;
                break;
            case NODE_CONTAINER:
                result = expand_md_body(ctx, child, indent, msb, node_depth+1);
                if(result) return result;
                break;
            default:
                result = expand_node(ctx, child, indent, msb, node_depth+1);
                if(result) return result;
                break;
        }
    }
    return result;
}

static
int
expand_table_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth){
    if(node_depth > DNDC_MAX_NODE_DEPTH){
        NODE_LOG_ERROR(ctx, n, "Max node depth exceeded: ", node_depth);
        return DNDC_ERROR_INVALID_TREE;
    }
    int result = 0;
    NODE_CHILDREN_FOR_EACH(t, n){
        Node* row = get_node(ctx, *t);
        if(row->type != NODE_TABLE_ROW){
            NODE_LOG_ERROR(ctx, row, "Expected table row, got ", quotedls(NODENAMES[row->type]));
            return DNDC_ERROR_INVALID_TREE;
        }
        msb_write_nchar(msb, ' ', indent);
        size_t i = 0;
        NODE_CHILDREN_FOR_EACH(item, row){
            if(i){
                msb_write_literal(msb, " | ");
            }
            i++;
            Node* row_item = get_node(ctx, *item);
            if(row_item->type == NODE_STRING){
                msb_write_str(msb, row_item->header.text, row_item->header.length);
            }
            else if(row_item->type == NODE_CONTAINER){
                bool first = true;
                NODE_CHILDREN_FOR_EACH(s, row_item){
                    if(!first){
                        if(row_item->col > indent){
                            msb_write_nchar(msb, ' ', row_item->col);
                        }
                        else {
                            // exact number is a guess, idk.
                            msb_write_nchar(msb, ' ', indent + node_children_count(row)*4);
                        }
                    }
                    first = false;
                    Node* container_item = get_node(ctx, *s);
                    if(container_item->type != NODE_STRING){
                        NODE_LOG_ERROR(ctx, row, "Expected string, got ", quotedls(NODENAMES[row->type]));
                        return DNDC_ERROR_INVALID_TREE;
                    }
                    msb_write_str(msb, container_item->header.text, container_item->header.length);
                    msb_write_char(msb, '\n');
                }
            }
            else {
                NODE_LOG_ERROR(ctx, row, "Expected string or container, got ", quotedls(NODENAMES[row->type]));
                return DNDC_ERROR_INVALID_TREE;
            }
        }
        msb_write_char(msb, '\n');
    }
    return result;
}

static
int
expand_keyvalue_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int node_depth){
    if(node_depth > DNDC_MAX_NODE_DEPTH){
        NODE_LOG_ERROR(ctx, n, "Max node depth exceeded: ", node_depth);
        return DNDC_ERROR_INVALID_TREE;
    }
    int result = 0;
    NODE_CHILDREN_FOR_EACH(c, n){
        Node* child = get_node(ctx, *c);
        if(child->type != NODE_KEYVALUEPAIR){
            result = expand_node(ctx, child, indent, msb, node_depth+1);
            if(result) return result;
            continue;
        }
        if(node_children_count(child) != 2){
            NODE_LOG_ERROR(ctx, child, LS("keyvalue pair node needs exactly two children, has:"), node_children_count(child));
            return DNDC_ERROR_INVALID_TREE;
        }
        NodeHandle* handles = node_children(child);
        Node* key = get_node(ctx, handles[0]);
        Node* value = get_node(ctx, handles[1]);
        if(key->type != NODE_STRING){
            NODE_LOG_ERROR(ctx, key, SV("Expected string for key, got "), quotedls(NODENAMES[key->type]));
            return DNDC_ERROR_INVALID_TREE;
        }
        msb_write_nchar(msb, ' ', indent);
        MSB_FORMAT(msb, key->header, ": ");
        if(value->type == NODE_STRING){
            msb_write_str(msb, value->header.text, value->header.length);
            msb_write_char(msb, '\n');
        }
        else if(value->type == NODE_CONTAINER){
            bool first = true;
            NODE_CHILDREN_FOR_EACH(v, value){
                Node* vchild = get_node(ctx, *v);
                if(vchild->type != NODE_STRING){
                    NODE_LOG_ERROR(ctx, vchild, SV("Expected string for value, got "), quoted(LS_to_SV(NODENAMES[key->type])));
                }
                if(!first){
                    if(vchild->col > indent){
                        msb_write_nchar(msb, ' ', vchild->col);
                    }
                    else {
                        msb_write_nchar(msb, ' ',  indent + key->header.length+2);
                    }
                }
                first = false;
                msb_write_str(msb, vchild->header.text, vchild->header.length);
                msb_write_char(msb, '\n');
            }
        }
        else {
            NODE_LOG_ERROR(ctx, value, SV("Expected string or container for value, got "), quoted(LS_to_SV(NODENAMES[value->type])));
            return DNDC_ERROR_INVALID_TREE;
        }
    }
    return result;
}

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
