#ifndef DNDC_EXPAND_C
#define DNDC_EXPAND_C
#include "dndc.h"
#include "dndc_types.h"
#include "dndc_funcs.h"
#include "MStringBuilder.h"
#include "error_handling.h"

#ifdef __clang__
#pragma clang assume_nonnull begin
#endif

static
Errorable_f(void)
expand_node_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb);

static
Errorable_f(void)
expand_node(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb);

static
Errorable_f(void)
expand_md_list(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb);

static
Errorable_f(void)
expand_md_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb);
static
Errorable_f(void)
expand_table_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb);
static
Errorable_f(void)
expand_keyvalue_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb);

static
Errorable_f(void)
expand_to_dnd(DndcContext*ctx, MStringBuilder* msb){
    Errorable(void) result = {0};
    Node* node = get_node(ctx, ctx->root_handle);
    if(node->type != NODE_MD){
        result = expand_node(ctx, node, 0, msb);

        // node_set_err_q(ctx, node, SV("Expected md, got "), NODETYPE_TO_NODE_ALIASES[node->type]);
        // Raise(PARSE_ERROR);
        }
    else
        result = expand_node_body(ctx, node, 0, msb);
    return result;
    }

static 
void
write_generic_header(Node* n, int indent, MStringBuilder*msb){
    msb_write_nchar(msb, ' ', indent);
    if(n->header.length)
        msb_write_str(msb, n->header.text, n->header.length);
    const StringView* hd = &NODETYPE_TO_NODE_ALIASES[n->type];
    MSB_FORMAT(msb, "::", *hd);
    RARRAY_FOR_EACH(at, n->attributes){
        MSB_FORMAT(msb, " @", at->key);
        if(at->value.length){
            MSB_FORMAT(msb, "(", at->value, ")");
            }
        }
    RARRAY_FOR_EACH(cls, n->classes){
        MSB_FORMAT(msb, " .", *cls);
        }
    msb_write_char(msb, '\n');
    }

static
Errorable_f(void)
expand_node(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb){
    Errorable(void) result = {0};
    switch(n->type){
        case NODE_STRING:
            msb_write_nchar(msb, ' ', indent);
            if(n->header.length)
                msb_write_str(msb, n->header.text, n->header.length);
            msb_write_char(msb, '\n');
            return result;
        case NODE_STYLESHEETS:
        case NODE_SCRIPTS:
            write_generic_header(n, indent, msb);
            return expand_node_body(ctx, n, indent+2, msb);
        case NODE_TITLE:
        case NODE_HEADING:
            write_generic_header(n, indent, msb);
            if(node_children_count(n)){
                node_set_err(ctx, n, LS("TITLE or HEADING has children"));
                Raise(PARSE_ERROR);
                }
            return result;
        case NODE_KEYVALUEPAIR:
        case NODE_LIST_ITEM:
        case NODE_LIST:
        case NODE_JS:
        case NODE_BULLETS:
        // case NODE_IMPORT:
        case NODE_TABLE_ROW:
        case NODE_PARA:
            node_set_err_q(ctx, n, SV("Node escaped to top level: "), NODETYPE_TO_NODE_ALIASES[n->type]);
            Raise(PARSE_ERROR);
        case NODE_META:
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
            write_generic_header(n, indent, msb);
            return expand_node_body(ctx, n, indent+2, msb);
        case NODE_DATA:
            node_set_err(ctx, n, LS("DATA_NODE unhandled"));
            Raise(PARSE_ERROR);
        case NODE_HR:
        case NODE_NAV:
            write_generic_header(n, indent, msb);
            if(node_children_count(n)){
                node_set_err(ctx, n, LS("NAV or HR has children"));
                Raise(PARSE_ERROR);
                }
            return result;
        case NODE_IMPORT:
        case NODE_CONTAINER:{
            NODE_CHILDREN_FOR_EACH(child, n){
                result = expand_node(ctx, get_node(ctx, *child), indent, msb);
                if(result.errored) return result;
                }
            return result;
            }break;
        case NODE_INVALID:
            node_set_err(ctx, n, LS("INVALID_NODE"));
            Raise(PARSE_ERROR);
        }
    unreachable();
    }

static
Errorable_f(void)
expand_node_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb){
    Errorable(void) result = {0};
    switch(n->type){
        case NODE_DETAILS:
        case NODE_MD:
            result = expand_md_body(ctx, n, indent, msb);
            return result;
        // Should I read the file for --expand?
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
                result = expand_node(ctx, child, indent, msb);
                if(result.errored) return result;
                }
            return result;
        case NODE_TABLE:
            return expand_table_body(ctx, n, indent, msb);
        case NODE_KEYVALUE:
            return expand_keyvalue_body(ctx, n, indent, msb);
        case NODE_INVALID:
        case NODE_HR:
        case NODE_CONTAINER:
        case NODE_DATA:
        case NODE_NAV:
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
            node_set_err_q(ctx, n, SV("Node can't be expanded into text format: "), NODETYPE_TO_NODE_ALIASES[n->type]);
            Raise(PARSE_ERROR);
        }
    unreachable();
    }
static
Errorable_f(void)
expand_md_list(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb);
static
Errorable_f(void)
expand_md_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb);
static
Errorable_f(void)
expand_md_bullets(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb, int depth){
    Errorable(void) result = {0};
    NODE_CHILDREN_FOR_EACH(l, n){
        Node* li = get_node(ctx, *l);
        if(li->type != NODE_LIST_ITEM){
            node_set_err(ctx, li, LS("non list-item child of bullets"));
            Raise(PARSE_ERROR);
            }
        msb_write_nchar(msb, ' ', indent);
        switch(depth){
            case 0:  msb_write_literal(msb, "* "); break;
            case 1:  msb_write_literal(msb, "+ "); break;
            default: msb_write_literal(msb, "- "); break;
            }
        size_t count = node_children_count(li);
        if(!count){
            node_set_err(ctx, li, LS("List Item must have at least one child"));
            Raise(PARSE_ERROR);
            }
        size_t i = 0;
        NODE_CHILDREN_FOR_EACH(subitem, li){
            Node* sub = get_node(ctx, *subitem);
            if(i == 0){
                if(sub->type != NODE_STRING){
                    node_set_err(ctx, sub, LS("First List Item must be a string"));
                    Raise(PARSE_ERROR);
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
                result = expand_md_bullets(ctx, sub, indent+2, msb, depth+1);
                if(result.errored) return result;
                }
            else if(sub->type == NODE_LIST){
                result = expand_md_list(ctx, sub, indent+2, msb);
                if(result.errored) return result;
                }
            else {
                node_set_err(ctx, sub, LS("List items must contain strings, bullets or lists"));
                Raise(PARSE_ERROR);
                }
            i++;
            }
        }
    return result;
    }
static
Errorable_f(void)
expand_md_list(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb){
    Errorable(void) result = {0};
    size_t list_number = 0;
    NODE_CHILDREN_FOR_EACH(l, n){
        list_number++;
        Node* li = get_node(ctx, *l);
        if(li->type != NODE_LIST_ITEM){
            node_set_err(ctx, li, LS("non list-item child of list"));
            Raise(PARSE_ERROR);
            }
        msb_write_nchar(msb, ' ', indent);
        MSB_FORMAT(msb, list_number, ". ");
        size_t count = node_children_count(li);
        if(!count){
            node_set_err(ctx, li, LS("List Item must have at least one child"));
            Raise(PARSE_ERROR);
            }
        size_t i = 0;
        NODE_CHILDREN_FOR_EACH(subitem, li){
            Node* sub = get_node(ctx, *subitem);
            if(i == 0){
                if(sub->type != NODE_STRING){
                    node_set_err(ctx, sub, LS("First List Item must be a string"));
                    Raise(PARSE_ERROR);
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
                result = expand_md_bullets(ctx, sub, indent+2, msb, 0);
                if(result.errored) return result;
                }
            else if(sub->type == NODE_LIST){
                result = expand_md_list(ctx, sub, indent+2, msb);
                if(result.errored) return result;
                }
            else {
                node_set_err(ctx, sub, LS("List items must contain strings, bullets or lists"));
                Raise(PARSE_ERROR);
                }
            i++;
            }
        }
    return result;

    }
static
Errorable_f(void)
expand_md_para(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb){
    Errorable(void) result = {0};
    NODE_CHILDREN_FOR_EACH(c, n){
        Node* child = get_node(ctx, *c);
        if(child->type != NODE_STRING){
            node_set_err(ctx, child, LS("Expected string node in md para"));
            Raise(PARSE_ERROR);
            }
        msb_write_nchar(msb, ' ', indent);
        MSB_FORMAT(msb, child->header, "\n");
        }
    msb_write_char(msb, '\n');
    return result;
    }

static
Errorable_f(void)
expand_md_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb){
    Errorable(void) result = {0};
    NODE_CHILDREN_FOR_EACH(c, n){
        Node* child = get_node(ctx, *c);
        switch(child->type){
            case NODE_BULLETS:
                result = expand_md_bullets(ctx, child, indent, msb, 0);
                if(result.errored) return result;
                break;
            case NODE_PARA:
                result = expand_md_para(ctx, child, indent, msb);
                if(result.errored) return result;
                break;
            case NODE_LIST:
                result = expand_md_list(ctx, child, indent, msb);
                if(result.errored) return result;
                break;
            default:
                result = expand_node(ctx, child, indent, msb);
                if(result.errored) return result;
                break;
            }
        }
    return result;
    }

static
Errorable_f(void)
expand_table_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb){
    Errorable(void) result = {0};
    NODE_CHILDREN_FOR_EACH(t, n){
        Node* row = get_node(ctx, *t);
        if(row->type != NODE_TABLE_ROW){
            node_set_err_q(ctx, row, SV("Expected table row, got "), NODETYPE_TO_NODE_ALIASES[row->type]);
            Raise(PARSE_ERROR);
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
                        node_set_err_q(ctx, row, SV("Expected string, got "), NODETYPE_TO_NODE_ALIASES[row->type]);
                        Raise(PARSE_ERROR);
                        }
                    msb_write_str(msb, container_item->header.text, container_item->header.length);
                    msb_write_char(msb, '\n');
                    }
                }
            else {
                node_set_err_q(ctx, row, SV("Expected string or container, got "), NODETYPE_TO_NODE_ALIASES[row->type]);
                Raise(PARSE_ERROR);
                }
            }
        msb_write_char(msb, '\n');
        }
    return result;
    }

static
Errorable_f(void)
expand_keyvalue_body(DndcContext*ctx, Node* n, int indent, MStringBuilder*msb){
    Errorable(void) result = {0};
    NODE_CHILDREN_FOR_EACH(c, n){
        Node* child = get_node(ctx, *c);
        if(child->type != NODE_KEYVALUEPAIR){
            result = expand_node(ctx, child, indent, msb);
            if(result.errored) return result;
            continue;
            }
        if(node_children_count(child) != 2){
            node_set_err(ctx, child, LS("keyvalue pair node needs exactly two children"));
            Raise(PARSE_ERROR);
            }
        NodeHandle* handles = node_children(child);
        Node* key = get_node(ctx, handles[0]);
        Node* value = get_node(ctx, handles[1]);
        if(key->type != NODE_STRING){
            node_set_err_q(ctx, key, SV("Expected string for key, got "), NODETYPE_TO_NODE_ALIASES[key->type]);
            Raise(PARSE_ERROR);
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
                    node_set_err_q(ctx, vchild, SV("Expected string for value, got "), NODETYPE_TO_NODE_ALIASES[key->type]);
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
            node_set_err_q(ctx, value, SV("Expected string or container for value, got "), NODETYPE_TO_NODE_ALIASES[value->type]);
            Raise(PARSE_ERROR);
            }
        }
    return result;
    }

#ifdef __clang__
#pragma clang assume_nonnull end
#endif

#endif
