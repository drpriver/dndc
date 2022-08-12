//
// JS API
// ------
// This is the documentation of the js API for scripting the
// document.  It is presented in typescript syntax.  In addition
// to these, many of the usual Javascript functions and objects
// are available, such as `JSON.stringify`, `Map`s, etc.
//

//
// CtxType
// -------
// The type of the document context object, `ctx`.
type CtxType = {
    //
    // root
    // ----
    // The root of the document. You can assign nodes to this
    // field.
    root: Node?;

    //
    // outfile
    // -------
    // Basename of the output file. If the output path is
    // /foo/bar/baz.html, this will be baz.html.
    readonly outfile: string;

    //
    // outdir
    // ------
    // Directory of the output file. If the output path is
    // /foo/bar/baz.html, this will be /foor/bar
    readonly outdir: string;

    //
    // outpath
    // -------
    // The path to the output file.
    readonly outpath: string;

    //
    // sourcepath
    // ----------
    // Path to the input file.
    readonly sourcepath: string;

    //
    // base
    // ----
    // Relative paths are relative to this directory.
    readonly base: string;

    //
    // all_nodes
    // ---------
    // An array containing all of the nodes in the document at the
    // time this field is accessed. Mutating this array does not
    // change what nodes are in the document.
    all_nodes: Array<Node>;

    //
    // make_string
    // -----------
    // Creates a string node with the given string as content.
    //
    // Arguments:
    // ----------
    //   s: The string contents of the node to be.
    //
    // Returns:
    // --------
    // The new STRING node.
    //
    // Example:
    // --------
    //   let hello = ctx.make_string('Hello world!');
    //   ctx.root.add_child(hello);
    // --------
    make_string(s:string): Node;

    //
    // make_node
    // ---------
    // Creates a new node with no parents or children. Specify the
    // type as one of the predefined constants in `NodeType`.
    // Options is, well, optional and allows you to specify those
    // things without needing to assign them in separate
    // statements.
    //
    // Arguments:
    // ----------
    //   type:    The type of the new node, from NodeType.
    //   options: Optional set of attributes for the new node.
    //      header:     If given, the header of the new node.
    //      classes:    If given, the classes of the new node.
    //      attributes: If given, the attributes of the new node.
    //                  Each attribute will have an empty string
    //                  as its key.
    //
    // Example:
    // --------
    //   let container = ctx.make_node(NodeType.DIV,
    //       {classes:['container'],
    //        header:'Table of Contents'});
    //   let toc = ctx.make_node(NodeType.TOC);
    //   container.add_child(toc);
    //   ctx.root.add_child(container);
    // --------
    make_node(type:number, options:{header:string?, classes:Array<string>?,
              attributes:Array<string>?}): Node;

    //
    // add_dependency
    // --------------
    // Mark the given path as a dependency of this document. This
    // is used if the dependencies of the document are outputed,
    // for example for integration with make to rebuild documents
    // when file are edited.  Normally you don't need to call this
    // as loading files will implicitly mark that file as a
    // dependency, but it is possible to indirectly depend on
    // files.
    //
    // Arguments:
    // ----------
    //   path: The path to add as a dependency of this document.
    //
    // Example:
    // --------
    //   ctx.add_dependency('Build/frazzle.html');
    // --------
    add_dependency(path:string);

    //
    // kebab
    // -----
    // A utility function that turns a string into the version of
    // the string that can be used as an ID. It will turn a string
    // looking like "This is a string" into "this-is-a-string".
    // If you access the '.id' field of a node, it will already be
    // kebabed, but you might be indirectly referring to ids by strings.
    //
    // This can also be useful just to see how ids will be
    // transformed.
    //
    // Arguments:
    // ----------
    //   s: The string to kebab
    //
    // Returns:
    // --------
    // The kebabed version of s.
    //
    // Example:
    // --------
    //   let kebabed = ctx.kebab('This is some text!');
    //   console.log(kebabed); // "this-is-some-text"
    // --------
    kebab(s:string): string;

    // html_escape
    // -----
    // Escapes the special characters for html. This is needed
    // for procedurally generating raw nodes as they are not
    // escaped. Regular nodes are escaped as part of the html
    // generation process.
    //
    // Arguments:
    // ----------
    //   s: The string to escape
    //
    // Returns:
    // --------
    // The escaped version of s.
    //
    // Example:
    // --------
    //   let escaped = ctx.html_escape('<script>alert("hello")</script>');
    //   let raw = ctx.root.make_child(NodeType.RAW);
    //   raw.add_child(escaped);
    // --------
    html_escape(s:string): string;

    //
    // select_nodes
    // ------------
    // Queries for nodes matching the given criteria. The result
    // will be an AND of all of the criteria.  Giving no arguments
    // selects all nodes in the document.
    //
    // Arguments:
    // ----------
    //   type:       The type of the selected nodes. Use a constant
    //               from NodeType. Leave this out to not restrict by
    //               nodetype.
    //   classes:    Array of class names. All nodes returned will
    //               have all of these classes.
    //   attributes: Array of attribute keys. All nodes returned
    //               will have all of these attributes.
    //   id:         String id of the node.
    //
    // Returns:
    // --------
    // The array of nodes matching all of the given criteria.
    // This can be the empty array.
    //
    // Example:
    // -------
    //   let rooms = ctx.select_nodes({classes:['room']});
    //   for(let room of rooms)
    //     room.classes.add('dungeon');
    // -------
    select_nodes(args:{type:number?, classes:Array<string>?,
                       attributes:Array<string>?, id:string?}): Array<Node>;

    //
    // add_link
    // --------
    // Adds the key, value pair to the link table for resolving
    // what square bracket links link to.
    //
    // Arguments:
    // ----------
    //   key:   The text that will be inside [] style links.
    //          Note that this gets "kebabed", so that case folding
    //          and differences in punctuation map to the same
    //          thing.
    //   value: The url that is the target of the link.
    //
    // Example:
    // --------
    //   let rooms = ctx.select_nodes({classes:['room']});
    //   // Room header is of format "1. Some name"
    //   for(let room of rooms){
    //     let [number, name] = room.header.split('.');
    //     ctx.add_link(`room ${number}`, room.id);
    //     ctx.add_link(name, room.id);
    //   }
    //   // Now you can link to rooms by number or by name.
    // --------
    add_link(key:string, value: string);

    toString(): string;
}
//
// ctx
// ---
// The actual ctx that you refer to in your scripts.
export const ctx: CtxType;

//
// FileSystemT
// -----------
type FileSystemT = {
    // Note:
    // All of these file system functions are relative to the
    // `base` of the ctx.

    //
    // load_file
    // ---------
    // Loads the file located at the given path as a string.
    //
    // Arguments:
    // ----------
    //   path: Path to the file to load. If this is a relative
    //         path, it will be relative to ctx.base, which is
    //         usually the folder that the main dnd file is in.
    // Returns:
    // --------
    // The text of the indicated file.
    //
    // Example:
    // --------
    //   let text = FileSystem.load_file('some random file');
    //   // do some preprocessing with the text or whatever
    //   let raw = ctx.make_node(NodeType.RAW);
    //   raw.add_child(text);
    //   ctx.root.add_child(raw);
    // --------
    load_file(path:string): string;

    //
    // load_file_as_base64
    // -------------------
    // Loads the file located at the given path, base64ing the
    // contained bytes.  This is useful for embedding binary data
    // in the document (like wasm) that is then converted back to
    // binary when the document loads.
    //
    // Arguments:
    // ----------
    //   path: Path to the file to load. If this is a relative
    //         path, it will be relative to ctx.base, which is
    //         usually the folder that the main dnd file is in.
    // Returns:
    // --------
    // The contents of the given file, encoded in base64.
    //
    // Example:
    // --------
    //   let wasm = FileSystem.load_file_as_base64('example.wasm');
    //   let script = ctx.make_node(NodeType.SCRIPTS, {attributes:['inline']});
    //   script.add_child(`let wasmb64 = "${wasm}";`);
    //   ctx.root.add_child(script);
    // --------
    load_file_as_base64(path:string): string;

    //
    // list_dnd_files
    // --------------
    // Recursively lists all dnd files (files ending with .dnd).
    // Recursive means it will find them in subfolders as well. If
    // not given an argument, does the current directory.
    // Otherwise, scans the given directory.
    //
    // Arguments:
    // ----------
    //   path: (optional). If given, which directory to start the
    //         recursive search for dnd files.
    // Returns:
    // --------
    // An array of paths to dnd files. these paths will be
    // relative to ctx.base if possible.
    //
    // Example:
    // --------
    //   let dnd_files = FileSystem.list_dnd_files();
    //   let text = '';
    //   for(let dnd of dnd_files){
    //     ctx.add_link(dnd.replace('.dnd', ''), dnd.replace('.dnd', '.html'));
    //     text += `* [${dnd.replace('.dnd', '')}]\n`;
    //   }
    //   ctx.root.parse(text);
    // --------
    list_dnd_files(path:string?): Array<string>;

    //
    // exists
    // ------
    // Checks if there is a file or folder at the given path,
    // returning true if it does exist.
    //
    // Arguments:
    // ----------
    //   path: Path to check for existence. If this is a relative
    //         path, it is relative to ctx.base.
    //
    // Returns:
    // --------
    // Whether the path exists or not.
    //
    // Example:
    // --------
    //   if(FileSystem.exists('foo.dnd')){
    //     ctx.add_link('foo', 'foo.html');
    //     ctx.root.parse('Go to [foo].');
    //   }
    // --------
    exists(path:string): boolean;

    //
    // write_file
    // ----------
    // Writes out a new file to the given path with the given
    // content.
    //
    // NOTE: This method is normally disabled and must be explictly
    //       enabled by the host.
    //
    // Arguments:
    // ----------
    //   path:    Path to file to write. If this is a relative
    //            path, it is relative to ctx.base.
    //   content: The text to write into the file.
    // Example:
    // --------
    //   // FIXME: this example doesn't make sense
    //
    //   // Create an interface file for linking.
    //   let to_link = ctx.select_nodes({attributes:['linkme']});
    //   let s  = '::links\n';
    //   for(let tl of to_link)
    //     s += `  ${tl.header} = #${tl.id}\n`;
    //   FileSystem.write_file(Args[0], s);
    // --------
    write_file(path:string, content:string);
};

//
// FileSystem
// ----------
// The actual FileSystem that you refer to in your scripts
export const FileSystem: FileSystemT;

//
// NodeLocation
// ------------
// Where the node is in the document
type NodeLocation = {
    readonly file: string;
    readonly line: number;
    readonly column: number;
}


//
// Node
// ----
// This is the type of the nodes, which can be accessed via the
// context and also by the `node` variable that is implicitly
// placed into each js block.
type Node = {
    //
    // parent
    // ------
    // The node that is the parent of this node (above it in the
    // document).  This will be null if this node is the root or
    // if this node is an orphan (new node or detached node).
    parent: Node?;

    //
    // type
    // ----
    // The type of the node. See the NodeType object for what the
    // types are.
    type: number;

    //
    // children
    // --------
    // Array of children of this node. Note that mutating this
    // array does not stick. It is regenerated each time this
    // field is accessed. Use the `add_child`, `replace_child`,
    // `insert_child`, or have the child detach itself in order to
    // actually change the children.
    children: Array<Node>;


    //
    // header
    // ------
    // The header of the node is either the string content for a
    // STRING node, or it will be the heading of that node.
    header: string;

    //
    // id
    // --
    // The id of the node, generated from the header or explicitly
    // set.  You can explicitly set this field. The id will always
    // be "kebabed".
    id: string;

    //
    // noinline
    // --------
    // The noinline flag, presented as a boolean
    noinline: boolean;

    //
    // noid
    // ----
    // If true, the node will not be assigned an id when rendered.
    noid: boolean;

    //
    // hide
    // ----
    // If true, the node will not be rendered in the output.
    hide: boolean;


    //
    // location
    // --------
    // Where in the document the node is.
    readonly location: NodeLocation;

    //
    // parse
    // -----
    // Parse the string as a .dnd file and append the top level
    // nodes as children of this node.
    //
    // Arguments:
    // ----------
    //   dnd: A string that represents dnd source contents.
    //
    // Example:
    // --------
    //   let names = ['Jon', 'Joe', 'Homer'];
    //   let text = '';
    //   for(let name of names)
    //     text += `* ${name}\n`;
    //   ctx.root.parse(text);
    // --------
    parse(dnd:string);

    //
    // detach
    // ------
    // Remove this node from its parent and sets its parent to
    // null.  Call this before adding this node as a child of
    // another node or making it the root of the document.
    //
    // Example:
    // --------
    //   let root = ctx.root;
    //   root.detach();
    //   let new_root = ctx.make_node(NodeType.DIV);
    //   new_root.add_child(ctx.make_node(NodeType.TOC));
    //   new_root.add_child(root);
    //   ctx.root = new_root;
    // --------
    detach();

    //
    // make_child
    // ---------
    // Creates a new node whose parent is this node. This works
    // like ctx.make_node in all other respects.
    // Specify the type as one of the predefined constants in
    // `NodeType`.  Options is, well, optional and allows you
    // to specify those things without needing to assign them
    // in separate statements.
    //
    // Arguments:
    // ----------
    //   type:    The type of the new node, from NodeType.
    //   options: Optional set of attributes for the new node.
    //      header:     If given, the header of the new node.
    //      classes:    If given, the classes of the new node.
    //      attributes: If given, the attributes of the new node.
    //                  Each attribute will have an empty string
    //                  as its key.
    //
    // Example:
    // --------
    //   let container = ctx.root.make_child(NodeType.DIV,
    //       {classes:['container'],
    //        header:'Table of Contents'});
    //   container.make_child(NodeType.TOC);
    // --------
    make_child(type:number, options:{header:string?, classes:Array<string>?,
              attributes:Array<string>?}): Node;

    //
    // add_child
    // ---------
    // Append the given node to the end of the children.
    // As a convenient special case, strings can be appended. This
    // translates to creating a STRING node and then immediately
    // adding it to this node
    //
    // Arguments:
    // ----------
    //   node: The node to add as a child of this node. It will
    //         appear in subsequent accesses to .children.
    //
    // Example:
    // --------
    //   let wrapper = ctx.make_node(NodeType.DIV, {classes:['wrapper']});
    //   let root = ctx.root;
    //   root.detach();
    //   wrapper.add_child('Hello World');
    //   wrapper.add_child(root);
    //   ctx.root = wrapper;
    // --------
    add_child(node:Node|string);

    //
    // replace_child
    // -------------
    // Replace the given child.
    //
    // Arguments:
    // ----------
    //   old: The node to replace that is currently a child of
    //        this node.
    //   new: The node to replce that old node with.
    //
    // Example:
    // --------
    //   let tables = ctx.select_nodes(NodeType.TABLE);
    //   for(let table of tables){
    //     let container = ctx.make_node(NodeType.DIV, {header:table.header});
    //     table.header = '';
    //     table.parent.replace_child(table, container);
    //     container.add_child(table);
    //   }
    // --------
    replace_child(old:Node, new:Node|string);

    //
    // insert_child
    // ------------
    // Insert the node at the given index, sliding all the nodes
    // at that index and later down by 1. If where is greater than
    // or equal to the number of child nodes, than this just acts
    // like `add_child`.
    //
    // Arguments:
    // ----------
    //   where: What index to insert the node into.
    //   node:  The node to insert.
    //
    // Example:
    // --------
    //   let h = ctx.make_node(NodeType.HEADING, {header:'An Example'});
    //   ctx.root.insert_child(0, h);
    // --------
    insert_child(where:number, node:Node|string);

    //
    // attributes
    // ----------
    // The key/value mapping of the attributes of this node. See
    // the type description below (`Attributes`).
    attributes: Attributes;

    //
    // classes
    // -------
    // The classes of this node (css classes). See the type
    // description below (`Classes`).
    classes: Classes;

    //
    // err
    // ---
    // Throw an error originating from this node with the given
    // message. Use this if the node itself is erroneous (you were
    // expecting certain text context for example). Do note that
    // you can also just `throw new Error('whatever')` if you want
    // the error to originate from this part of the script.
    //
    // Arguments:
    // ----------
    //   msg: The error message to include in the thrown
    //   exception.
    //
    // Example:
    // --------
    //   if(!node.header.includes('Monday'))
    //     node.err('Expected node to have Monday');
    // --------
    err(msg:string);

    //
    // has_class
    // ---------
    // Checks if a class is present or not in the classes.
    //
    // Note: this function might be moved to the Classes object in
    // the future.
    //
    // Example:
    // --------
    //   let rooms = [];
    //   for(let child of ctx.root.children){
    //     if(child.has_class('room'))
    //       rooms.push(child);
    //   }
    // --------
    has_class(cls:string): boolean;

    //
    // clone
    // -----
    // Dupe this node as an orphan. Attributes, classes and
    // headers are copied.  Somewhat strangely, this will keep the
    // child nodes, but as a shallow copy. They will not have
    // their parent nodes set to the clone. This is weird, but is
    // useful for having a part of the document tree in the
    // document twice.
    //
    // NOTE: This function may change. In particular, the above
    // behavior with the children might change.
    //
    // Example:
    // --------
    //   let old_root = ctx.root;
    //   old_root.detach();
    //   let clone = old_root.clone();
    //   let container = ctx.make_node(NodeType.DIV, classes:['container']);
    //   old_root.classes.add('left');
    //   container.add_child(old_root);
    //   clone.classes.add('right');
    //   container.add_child(clone);
    //   ctx.root = container;
    // --------
    clone(): Node;

    toString(): string;

    //
    // get
    // ---
    // If this is a KEYVALUE node, returns the first value associated with the
    // given key.
    //
    // If not a KEYVALUE node, throws a type error.
    get(key:string):string?;

    //
    // set
    // ---
    // If this is a KEYVALUE node, sets the first key that matches to the given
    // value. If the key can not be found, appends this as a key value pair.
    //
    // If not a KEYVALUE node, throws a type error.
    set(key:string, value:string);
}
//
// node
// ----
// In a js block, this variable represents the js block itself.
// You can access the containing element via the .parent field.
export const node: Node;

//
// Attributes
// ----------
type Attributes = {
    //
    // get
    // ---
    // Retrieve the value associated with the given attribute,
    // returning undefined if not present. Also note that
    // attributes don't need to have a value - this will be
    // returned as an empty string.
    //
    // Arguments:
    // ----------
    //   key: The attribute.
    //
    // Returns:
    // --------
    // Undefined if attribute is missing, empty string if it is
    // set to nothing, otherwise the value associated with the
    // attribute.
    //
    // Example:
    // --------
    //   let coords = {};
    //   let rooms = ctx.select_nodes(classes:['room']);
    //   for(let room of rooms){
    //     let coord = room.attributes.get('coord');
    //     let[x, y] = coord.split(',');
    //     x = parseInt(x);
    //     y = parseInt(y);
    //     coords[room.id] = [x, y];
    //   }
    // --------
    get(key:string):string?;

    //
    // has
    // ---
    // Returns whether the attribute is present.
    //
    has(key:string):boolean;

    //
    // set
    // ---
    // Sets the given attribute with the given value. If the value
    // is not given here, it is treated as an empty string.
    //
    // Example:
    // --------
    //   for(let n of ctx.select_nodes({classes:['room']})){
    //     let h = ctx.make_node(NodeType.HEADING, {header:'Hello'});
    //     h.attributes.set('noid');
    //     n.insert_child(0, h);
    //   }
    // --------
    set(key:string, value:string?);

    entries():Array<[string, string]>;
    toString():string;
    [Symbol.iterator]():Array<[string, string]>;
}

//
// Classes
// -------
type Classes = {

    //
    // append
    // ------
    // Add the class to this group.
    append(cls:string);

    values(): Array<string>;
    toString(): string;
    [Symbol.iterator](): Array<string>;
}

//
// Args
// ----
// This object is of a type that can be expressed in JSON. It is
// used to pass arguments from the calling process to js. If
// no args are given, it will be null. It is usually an array or
// an object.
//
export const Args: unknown;

//
// NodeType
// --------
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
    TOC:          number;
    COMMENT:      number;
    CONTAINER:    number;
    QUOTE:        number;
    JS:           number;
    DETAILS:      number;
    INVALID:      number;
    META:         number;
};
