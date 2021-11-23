//
// This is the documentation of the js API for scripting the
// document.  It is presented in typescript syntax.  In addition
// to these, many of the usual Javascript functions and objects
// are available, such as `JSON.stringify`, `Map`s, etc.
//

//
// The type of the document context object, `ctx`.
type CtxType = {
    //
    // The root of the document. You can assign nodes to this
    // field.
    root: Node?;

    //
    // Paths:
    //

    //
    // Basename of the output file. If the output path is
    // /foo/bar/baz.html, this will be baz.html.
    outfile: string;

    //
    // Directory of the output file. If the output path is
    // /foo/bar/baz.html, this will be /foor/bar
    outdir: string;

    //
    // The path to the output file.
    outpath: string;

    //
    // Path to the input file.
    sourcepath: string;

    //
    // Relative paths are relative to this directory.
    base: string;

    //
    // An array containing all of the nodes in the doucment at the
    // time this field is accessed. Mutating this array does not
    // change what nodes are in the document.
    all_nodes: Array<Node>;

    //
    // Creates a string node with the given string as content.
    make_string(_:string): Node;

    //
    // Creates a new node with no parents or children. Specify the
    // type as one of the predefined constants in `NodeType`.
    // Options is, well, optional and allows you to specify those
    // things without needing to assign them in separate
    // statements.
    make_node(type:number, options:{
                                header:string?,
                                classes:Array<string>?,
                                attributes:Array<string>?,
                                }): Node;

    //
    // Make the given path as a dependency of this document. This
    // is used if the dependencies of the document are outputed,
    // for example for integration with make to rebuild documents
    // when file are edited.  Normally you don't need to call this
    // as loading files will implicitly mark that file as a
    // dependency, but it is possible to indirectly depend on
    // files.
    add_dependency(_:string);

    //
    // A utility function that turns a string into the version of
    // the string that can be used as an ID.
    kebab(_:string): string;

    //
    // Sets a key, value in the generated data blob (which will be
    // available as `data_blob` in the global scope for scripts in
    // the final document.
    set_data(key:string, value:string);

    //
    // Queries for nodes matching the given criteria. The result
    // will be an AND of all of the criteria.  Giving no arguments
    // selects all nodes in the document.
    select_nodes(args:{type:number?, classes:Array<string>?,
                       attributes:Array<string>?}): Array<Node>;

    //
    // So you can console.log this object.
    toString(): string;

    //
    // Adds the key, value pair to the link table for resolving
    // what square bracket links link to.
    add_link(key:string, value: string);
}

// The actual ctx that you refer to in your scripts.
export const ctx: CtxType;

type FileSystemT = {
    // All of these file system functions are relative to the
    // `base` of the ctx.

    //
    // Loads the file located at the given path as a string.
    load_file(path:string): string;

    //
    // Loads the file located at the given path, base64ing the
    // contained bytes.  This is useful for embedding binary data
    // in the document (like wasm) that is then converting back to
    // binary when the document loads.
    load_file_as_base64(path:string): string;

    //
    // Recursively lists all dnd files (files ending with .dnd).
    // Recursive means it will find them in subfolders as well. If
    // not given an argument, does the current directory.
    // Otherwise, scans the given directory.
    list_dnd_files(path:string?): Array<string>;

    //
    // Checks if there is a file or folder at the given path,
    // returning true if it does exist.
    exists(path:string): boolean;
};

//
// The actual FileSystem that you refer to in your scripts
export const FileSystem: FileSystemT;


//
// This is the type of the nodes, which can be accessed via the
// context and also by the `node` variable that is implicitly
// placed into each js block.
type Node = {
    //
    // The node that is the parent of this node (above it in the
    // document).  This will be null if this node is the root or
    // if this node is an orphan (new node or detached node).
    parent: Node?;

    //
    // The type of the node. See the NodeType object for what the
    // types are.
    type: number;

    //
    // Array of children of this node. Note that mutating this
    // array does not stick. It is regenerated each time this
    // field is accessed. Use the `add_child`, `replace_child`,
    // `insert_child`, or have the child detach itself in order to
    // actually change the children.
    children: Array<Node>;

    //
    // So you can console.log this
    toString(): string;

    //
    // The header of the node is either the string content for a
    // STRING node, or it will be the heading of that node.
    header: string;

    //
    // The id of the node, generated from the header or explicitly
    // set.  You can explicitly set this field. The id will always
    // be "kebabed".
    id: string;

    //
    // Parse the string as a .dnd file and append the top level
    // nodes as children of this node.
    parse(_:string);

    //
    // Remove this node from its parent and sets its parent to
    // null.  Call this before adding this node as a child of
    // another node or making it the root of the document.
    detach();

    //
    // Append the given node to the end of the children.
    add_child(_:Node);

    //
    // Replace the given child.
    replace_child(old:Node, new:Node);

    //
    // Insert the node at the given index, sliding all the nodes
    // at that index and later down by 1. If where is greater than
    // or equal to the number of child nodes, than this just acts
    // like `add_child`.
    insert_child(where:numer, node:Node);

    //
    // The key/value mapping of the attributes of this node. See
    // the type description below (`Attributes`).
    attributes: Attributes;

    //
    // The classes of this node (css classes). See the type
    // description below (`Classes`).
    classes: Classes;

    //
    // Throw an error originating from this node with the given
    // message. Use this if the node itself is erroneous (you were
    // expecting certain text context for example). Do note that
    // you can also just `throw new Error('whatever')` if you want
    // the error to originate from this part of the script.
    err(_:string);

    //
    // Checks if a class is present or not in the classes.
    has_class(_:string): boolean;

    //
    // Dupe this node as an orphan. Attributes, classes and
    // headers are copied.  Somewhat strangely, this will keep the
    // child nodes, but as a shallow copy. They will not have
    // their parent nodes set to the clone. This is weird, but is
    // useful for having a part of the document tree in the
    // document twice.
    clone(): Node;
}
//
// In a js block, this variable represents the js block itself.
// You can access the containing element via the .parent field.
export const node: Node;

type Attributes = {
    //
    // Retrieve the value associated with the given attribute,
    // returning undefined if not present. Also note that
    // attributes don't need to have a value - this will be
    // returned as an empty string.
    get(key:string):string?;

    //
    // Returns whether the attribute is present.
    has(key:string):boolean;

    //
    // Sets the given attribute with the given value. If the value
    // is not given here, it is treated as an empty string.
    set(key:string, value:string?);

    //
    // So you can console.log
    toString():string;

    //
    // So you can iterate over it.
    entries():Array<[string, string]>;

    //
    // So you can iterate over it.
    [Symbol.iterator]():Array<[string, string]>;
}

type Classes = {
    //
    // So you can console.log
    toString(): string;

    //
    // Add the class to this group.
    append(_:string);

    //
    // So you can iterate over it.
    values(): Array<string>;

    //
    // So you can iterate over it.
    [Symbol.iterator](): Array<string>;
}

//
// These are the possible Node Types in the document.  It is
// possible to create a tree that doesn't make sense (A TABLE_ROW
// randomly as a child of STYLESHEETS). Don't do that. If you do,
// then either it will be ignored or an error will occur when the
// document is actually converted to html.
//
// The specific value are not guaranteed to be the same between
// versions, so always refer to them symbolically. In other words,
// do `NodeType.MD`, not whatever number it happens to be.
//
export const NodeType = {
    MD:           number;
    DIV:          number;
    STRING:       number;
    PARA:         number;
    TITLE:        number;
    HEADING:      number;
    TABLE:        number;
    TABLE_ROW:    number;
    STYLESHEETS:  number;
    DEPENDENCIES: number;
    LINKS:        number;
    SCRIPTS:      number;
    IMPORT:       number;
    IMAGE:        number;
    BULLETS:      number;
    RAW:          number;
    PRE:          number;
    LIST:         number;
    LIST_ITEM:    number;
    KEYVALUE:     number;
    KEYVALUEPAIR: number;
    IMGLINKS:     number;
    NAV:          number;
    DATA:         number;
    COMMENT:      number;
    TEXT:         number;
    CONTAINER:    number;
    QUOTE:        number;
    HR:           number;
    JS:           number;
    DETAILS:      number;
    INVALID:      number;
};
