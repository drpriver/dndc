type CtxType = {
    root: Node?;
    outfile: string;
    outdir: string;
    outpath: string;
    sourcepath: string;
    base: string;
    all_nodes: Array<Node>;
    make_string(_:string): Node;
    make_node(type:number, options:{header:string?, classes:Array<string>?, attributes:Array<string>?}): Node;
    add_dependency(_:string): Node;
    kebab(_:string): string;
    set_data(key:string, value:string);
    select_nodes(args:{type:number?, classes:Array<string>?, attributes:Array<string>?}): Array<Node>;
    toString(): string;
    add_link(key:string, value: string);
}

export const ctx: CtxType;

type FileSystemT = {
    load_file(path:string): string;
    load_file_as_base64(path:string): string;
};

export const FileSystem: FileSystemT;


type Node = {
    parent: Node?;
    type: number;
    children: Array<Node>;
    toString(): string;
    header: string;
    id: string;
    parse(_:string);
    detch();
    add_child(_:Node);
    replace_child(old:Node, new:Node);
    insert_child(where:numer, node:Node);
    attributes: Attributes;
    classes: Classes;
    err(_:string);
    has_class(_:string): boolean;
    clone(): Node;
}
export const node: Node;

type Attributes = {
    get(key:string):string?;
    has(key:string):boolean;
    set(key:string, value:string?);
    toString():string;
    entries():Array<[string, string]>;
    [Symbol.iterator]():Array<[string, string]>;
}

type Classes = {
    toString(): string;
    append(_:string);
    values(): Array<string>;
    [Symbol.iterator](): Array<string>;
}

export const NodeType = {
    MD          :number;
    DIV         :number;
    STRING      :number;
    PARA        :number;
    TITLE       :number;
    HEADING     :number;
    TABLE       :number;
    TABLE_ROW   :number;
    STYLESHEETS :number;
    DEPENDENCIES:number;
    LINKS       :number;
    SCRIPTS     :number;
    IMPORT      :number;
    IMAGE       :number;
    BULLETS     :number;
    RAW         :number;
    PRE         :number;
    LIST        :number;
    LIST_ITEM   :number;
    KEYVALUE    :number;
    KEYVALUEPAIR:number;
    IMGLINKS    :number;
    NAV         :number;
    DATA        :number;
    COMMENT     :number;
    TEXT        :number;
    CONTAINER   :number;
    QUOTE       :number;
    HR          :number;
    JS          :number;
    DETAILS     :number;
    INVALID     :number;
};
