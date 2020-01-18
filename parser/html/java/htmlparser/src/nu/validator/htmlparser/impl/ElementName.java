/*
 * Copyright (c) 2008-2017 Mozilla Foundation
 * Copyright (c) 2018-2020 Moonchild Productions
 * Copyright (c) 2020 Binary Outcast
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

package nu.validator.htmlparser.impl;

import java.util.Arrays;

import nu.validator.htmlparser.annotation.Inline;
import nu.validator.htmlparser.annotation.Local;
import nu.validator.htmlparser.annotation.NoLength;
import nu.validator.htmlparser.annotation.Unsigned;
import nu.validator.htmlparser.common.Interner;

public final class ElementName
// uncomment when regenerating self
//        implements Comparable<ElementName>
{

    /**
     * The mask for extracting the dispatch group.
     */
    public static final int GROUP_MASK = 127;

    /**
     * Indicates that the element is not a pre-interned element. Forbidden
     * on preinterned elements.
     */
    public static final int NOT_INTERNED = (1 << 30);

    /**
     * Indicates that the element is in the "special" category. This bit
     * should not be pre-set on MathML or SVG specials--only on HTML specials.
     */
    public static final int SPECIAL = (1 << 29);

    /**
     * The element is foster-parenting. This bit should be pre-set on elements
     * that are foster-parenting as HTML.
     */
    public static final int FOSTER_PARENTING = (1 << 28);

    /**
     * The element is scoping. This bit should be pre-set on elements
     * that are scoping as HTML.
     */
    public static final int SCOPING = (1 << 27);

    /**
     * The element is scoping as SVG.
     */
    public static final int SCOPING_AS_SVG = (1 << 26);

    /**
     * The element is scoping as MathML.
     */
    public static final int SCOPING_AS_MATHML = (1 << 25);

    /**
     * The element is an HTML integration point.
     */
    public static final int HTML_INTEGRATION_POINT = (1 << 24);

    /**
     * The element has an optional end tag.
     */
    public static final int OPTIONAL_END_TAG = (1 << 23);

    private @Local String name;

    private @Local String camelCaseName;

    /**
     * The lowest 7 bits are the dispatch group. The high bits are flags.
     */
    public final int flags;

    @Inline public @Local String getName() {
        return name;
    }

    @Inline public @Local String getCamelCaseName() {
        return camelCaseName;
    }

    @Inline public int getFlags() {
        return flags;
    }

    public int getGroup() {
        return flags & GROUP_MASK;
    }

    public boolean isInterned() {
        return (flags & NOT_INTERNED) == 0;
    }

    static ElementName elementNameByBuffer(@NoLength char[] buf, int offset, int length, Interner interner) {
        @Unsigned int hash = ElementName.bufToHash(buf, length);
        int index = Arrays.binarySearch(ElementName.ELEMENT_HASHES, hash);
        if (index < 0) {
            return null;
        } else {
            ElementName elementName = ElementName.ELEMENT_NAMES[index];
            @Local String name = elementName.name;
            if (!Portability.localEqualsBuffer(name, buf, offset, length)) {
                return null;
            }
            return elementName;
        }
    }

    /**
     * This method has to return a unique positive integer for each well-known
     * lower-cased element name.
     *
     * @param buf
     * @param len
     * @return
     */
    @Inline private static @Unsigned int bufToHash(@NoLength char[] buf, int length) {
        @Unsigned int len = length;
        @Unsigned int first = buf[0];
        first <<= 19;
        @Unsigned int second = 1 << 23;
        @Unsigned int third = 0;
        @Unsigned int fourth = 0;
        @Unsigned int fifth = 0;
        if (length >= 4) {
            second = buf[length - 4];
            second <<= 4;
            third = buf[length - 3];
            third <<= 9;
            fourth = buf[length - 2];
            fourth <<= 14;
            fifth = buf[length - 1];
            fifth <<= 24;
        } else if (length == 3) {
            second = buf[1];
            second <<= 4;
            third = buf[2];
            third <<= 9;
        } else if (length == 2) {
            second = buf[1];
            second <<= 24;
        }
        return len + first + second + third + fourth + fifth;
    }

    private ElementName(@Local String name, @Local String camelCaseName,
            int flags) {
        this.name = name;
        this.camelCaseName = camelCaseName;
        this.flags = flags;
    }

    public ElementName() {
        this.name = null;
        this.camelCaseName = null;
        this.flags = TreeBuilder.OTHER | NOT_INTERNED;
    }

    public void destructor() {
        // The translator adds refcount debug code here.
    }

    public void setNameForNonInterned(@Local String name) {
        // No need to worry about refcounting the local name, because in the
        // C++ case the scoped atom table remembers its own atoms.
        this.name = name;
        this.camelCaseName = name;
        assert this.flags == (TreeBuilder.OTHER | NOT_INTERNED);
    }

    // START CODE ONLY USED FOR GENERATING CODE uncomment and run to regenerate

//    /**
//     * @see java.lang.Object#toString()
//     */
//    @Override public String toString() {
//        return "(\"" + name + "\", \"" + camelCaseName + "\", " + decomposedFlags() + ")";
//    }
//
//    private String decomposedFlags() {
//        StringBuilder buf = new StringBuilder("TreeBuilder.");
//        buf.append(treeBuilderGroupToName());
//        if ((flags & SPECIAL) != 0) {
//            buf.append(" | SPECIAL");
//        }
//        if ((flags & FOSTER_PARENTING) != 0) {
//            buf.append(" | FOSTER_PARENTING");
//        }
//        if ((flags & SCOPING) != 0) {
//            buf.append(" | SCOPING");
//        }
//        if ((flags & SCOPING_AS_MATHML) != 0) {
//            buf.append(" | SCOPING_AS_MATHML");
//        }
//        if ((flags & SCOPING_AS_SVG) != 0) {
//            buf.append(" | SCOPING_AS_SVG");
//        }
//        if ((flags & OPTIONAL_END_TAG) != 0) {
//            buf.append(" | OPTIONAL_END_TAG");
//        }
//        return buf.toString();
//    }
//
//    private String constName() {
//        char[] buf = new char[name.length()];
//        for (int i = 0; i < name.length(); i++) {
//            char c = name.charAt(i);
//            if (c == '-') {
//                buf[i] = '_';
//            } else if (c >= '0' && c <= '9') {
//                buf[i] = c;
//            } else {
//                buf[i] = (char) (c - 0x20);
//            }
//        }
//        return new String(buf);
//    }
//
//    private int hash() {
//        return bufToHash(name.toCharArray(), name.length());
//    }
//
//    public int compareTo(ElementName other) {
//        int thisHash = this.hash();
//        int otherHash = other.hash();
//        if (thisHash < otherHash) {
//            return -1;
//        } else if (thisHash == otherHash) {
//            return 0;
//        } else {
//            return 1;
//        }
//    }
//
//    private String treeBuilderGroupToName() {
//        switch (getGroup()) {
//            case TreeBuilder.OTHER:
//                return "OTHER";
//            case TreeBuilder.A:
//                return "A";
//            case TreeBuilder.BASE:
//                return "BASE";
//            case TreeBuilder.BODY:
//                return "BODY";
//            case TreeBuilder.BR:
//                return "BR";
//            case TreeBuilder.BUTTON:
//                return "BUTTON";
//            case TreeBuilder.CAPTION:
//                return "CAPTION";
//            case TreeBuilder.COL:
//                return "COL";
//            case TreeBuilder.COLGROUP:
//                return "COLGROUP";
//            case TreeBuilder.FONT:
//                return "FONT";
//            case TreeBuilder.FORM:
//                return "FORM";
//            case TreeBuilder.FRAME:
//                return "FRAME";
//            case TreeBuilder.FRAMESET:
//                return "FRAMESET";
//            case TreeBuilder.IMAGE:
//                return "IMAGE";
//            case TreeBuilder.INPUT:
//                return "INPUT";
//            case TreeBuilder.ISINDEX:
//                return "ISINDEX";
//            case TreeBuilder.LI:
//                return "LI";
//            case TreeBuilder.LINK_OR_BASEFONT_OR_BGSOUND:
//                return "LINK_OR_BASEFONT_OR_BGSOUND";
//            case TreeBuilder.MATH:
//                return "MATH";
//            case TreeBuilder.META:
//                return "META";
//            case TreeBuilder.SVG:
//                return "SVG";
//            case TreeBuilder.HEAD:
//                return "HEAD";
//            case TreeBuilder.HR:
//                return "HR";
//            case TreeBuilder.HTML:
//                return "HTML";
//            case TreeBuilder.KEYGEN:
//                return "KEYGEN";
//            case TreeBuilder.NOBR:
//                return "NOBR";
//            case TreeBuilder.NOFRAMES:
//                return "NOFRAMES";
//            case TreeBuilder.NOSCRIPT:
//                return "NOSCRIPT";
//            case TreeBuilder.OPTGROUP:
//                return "OPTGROUP";
//            case TreeBuilder.OPTION:
//                return "OPTION";
//            case TreeBuilder.P:
//                return "P";
//            case TreeBuilder.PLAINTEXT:
//                return "PLAINTEXT";
//            case TreeBuilder.SCRIPT:
//                return "SCRIPT";
//            case TreeBuilder.SELECT:
//                return "SELECT";
//            case TreeBuilder.STYLE:
//                return "STYLE";
//            case TreeBuilder.TABLE:
//                return "TABLE";
//            case TreeBuilder.TEXTAREA:
//                return "TEXTAREA";
//            case TreeBuilder.TITLE:
//                return "TITLE";
//            case TreeBuilder.TEMPLATE:
//                return "TEMPLATE";
//            case TreeBuilder.TR:
//                return "TR";
//            case TreeBuilder.XMP:
//                return "XMP";
//            case TreeBuilder.TBODY_OR_THEAD_OR_TFOOT:
//                return "TBODY_OR_THEAD_OR_TFOOT";
//            case TreeBuilder.TD_OR_TH:
//                return "TD_OR_TH";
//            case TreeBuilder.DD_OR_DT:
//                return "DD_OR_DT";
//            case TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6:
//                return "H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6";
//            case TreeBuilder.OBJECT:
//                return "OBJECT";
//            case TreeBuilder.OUTPUT:
//                return "OUTPUT";
//            case TreeBuilder.MARQUEE_OR_APPLET:
//                return "MARQUEE_OR_APPLET";
//            case TreeBuilder.PRE_OR_LISTING:
//                return "PRE_OR_LISTING";
//            case TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U:
//                return "B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U";
//            case TreeBuilder.UL_OR_OL_OR_DL:
//                return "UL_OR_OL_OR_DL";
//            case TreeBuilder.IFRAME:
//                return "IFRAME";
//            case TreeBuilder.NOEMBED:
//                return "NOEMBED";
//            case TreeBuilder.EMBED:
//                return "EMBED";
//            case TreeBuilder.IMG:
//                return "IMG";
//            case TreeBuilder.AREA_OR_WBR:
//                return "AREA_OR_WBR";
//            case TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU:
//                return "DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU";
//            case TreeBuilder.FIELDSET:
//                return "FIELDSET";
//            case TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY:
//                return "ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY";
//            case TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR:
//                return "RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR";
//            case TreeBuilder.RB_OR_RTC:
//                return "RB_OR_RTC";
//            case TreeBuilder.RT_OR_RP:
//                return "RT_OR_RP";
//            case TreeBuilder.PARAM_OR_SOURCE_OR_TRACK:
//                return "PARAM_OR_SOURCE_OR_TRACK";
//            case TreeBuilder.MGLYPH_OR_MALIGNMARK:
//                return "MGLYPH_OR_MALIGNMARK";
//            case TreeBuilder.MI_MO_MN_MS_MTEXT:
//                return "MI_MO_MN_MS_MTEXT";
//            case TreeBuilder.ANNOTATION_XML:
//                return "ANNOTATION_XML";
//            case TreeBuilder.FOREIGNOBJECT_OR_DESC:
//                return "FOREIGNOBJECT_OR_DESC";
//            case TreeBuilder.MENUITEM:
//                return "MENUITEM";
//        }
//        return null;
//    }
//
//    /**
//     * Regenerate self
//     *
//     * @param args
//     */
//    public static void main(String[] args) {
//        Arrays.sort(ELEMENT_NAMES);
//        for (int i = 0; i < ELEMENT_NAMES.length; i++) {
//            int hash = ELEMENT_NAMES[i].hash();
//            if (hash < 0) {
//                System.err.println("Negative hash: " + ELEMENT_NAMES[i].name);
//                return;
//            }
//            for (int j = i + 1; j < ELEMENT_NAMES.length; j++) {
//                if (hash == ELEMENT_NAMES[j].hash()) {
//                    System.err.println("Hash collision: " + ELEMENT_NAMES[i].name
//                            + ", " + ELEMENT_NAMES[j].name);
//                    return;
//                }
//            }
//        }
//        for (int i = 0; i < ELEMENT_NAMES.length; i++) {
//            ElementName el = ELEMENT_NAMES[i];
//            System.out.println("public static final ElementName "
//                    + el.constName() + " = new ElementName" + el.toString()
//                    + ";");
//        }
//        System.out.println("private final static @NoLength ElementName[] ELEMENT_NAMES = {");
//        for (int i = 0; i < ELEMENT_NAMES.length; i++) {
//            ElementName el = ELEMENT_NAMES[i];
//            System.out.println(el.constName() + ",");
//        }
//        System.out.println("};");
//        System.out.println("private final static int[] ELEMENT_HASHES = {");
//        for (int i = 0; i < ELEMENT_NAMES.length; i++) {
//            ElementName el = ELEMENT_NAMES[i];
//            System.out.println(Integer.toString(el.hash()) + ",");
//        }
//        System.out.println("};");
//    }

    // START GENERATED CODE
    public static final ElementName AND = new ElementName("and", "and", TreeBuilder.OTHER);
    public static final ElementName ARG = new ElementName("arg", "arg", TreeBuilder.OTHER);
    public static final ElementName ABS = new ElementName("abs", "abs", TreeBuilder.OTHER);
    public static final ElementName BIG = new ElementName("big", "big", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName BDO = new ElementName("bdo", "bdo", TreeBuilder.OTHER);
    public static final ElementName CSC = new ElementName("csc", "csc", TreeBuilder.OTHER);
    public static final ElementName COL = new ElementName("col", "col", TreeBuilder.COL | SPECIAL);
    public static final ElementName COS = new ElementName("cos", "cos", TreeBuilder.OTHER);
    public static final ElementName COT = new ElementName("cot", "cot", TreeBuilder.OTHER);
    public static final ElementName DEL = new ElementName("del", "del", TreeBuilder.OTHER);
    public static final ElementName DFN = new ElementName("dfn", "dfn", TreeBuilder.OTHER);
    public static final ElementName DIR = new ElementName("dir", "dir", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName DIV = new ElementName("div", "div", TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
    public static final ElementName EXP = new ElementName("exp", "exp", TreeBuilder.OTHER);
    public static final ElementName GCD = new ElementName("gcd", "gcd", TreeBuilder.OTHER);
    public static final ElementName GEQ = new ElementName("geq", "geq", TreeBuilder.OTHER);
    public static final ElementName IMG = new ElementName("img", "img", TreeBuilder.IMG | SPECIAL);
    public static final ElementName INS = new ElementName("ins", "ins", TreeBuilder.OTHER);
    public static final ElementName INT = new ElementName("int", "int", TreeBuilder.OTHER);
    public static final ElementName KBD = new ElementName("kbd", "kbd", TreeBuilder.OTHER);
    public static final ElementName LOG = new ElementName("log", "log", TreeBuilder.OTHER);
    public static final ElementName LCM = new ElementName("lcm", "lcm", TreeBuilder.OTHER);
    public static final ElementName LEQ = new ElementName("leq", "leq", TreeBuilder.OTHER);
    public static final ElementName MTD = new ElementName("mtd", "mtd", TreeBuilder.OTHER);
    public static final ElementName MIN = new ElementName("min", "min", TreeBuilder.OTHER);
    public static final ElementName MAP = new ElementName("map", "map", TreeBuilder.OTHER);
    public static final ElementName MTR = new ElementName("mtr", "mtr", TreeBuilder.OTHER);
    public static final ElementName MAX = new ElementName("max", "max", TreeBuilder.OTHER);
    public static final ElementName NEQ = new ElementName("neq", "neq", TreeBuilder.OTHER);
    public static final ElementName NOT = new ElementName("not", "not", TreeBuilder.OTHER);
    public static final ElementName NAV = new ElementName("nav", "nav", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName PRE = new ElementName("pre", "pre", TreeBuilder.PRE_OR_LISTING | SPECIAL);
    public static final ElementName A = new ElementName("a", "a", TreeBuilder.A);
    public static final ElementName B = new ElementName("b", "b", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName RTC = new ElementName("rtc", "rtc", TreeBuilder.RB_OR_RTC | OPTIONAL_END_TAG);
    public static final ElementName REM = new ElementName("rem", "rem", TreeBuilder.OTHER);
    public static final ElementName SUB = new ElementName("sub", "sub", TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName SEC = new ElementName("sec", "sec", TreeBuilder.OTHER);
    public static final ElementName SVG = new ElementName("svg", "svg", TreeBuilder.SVG);
    public static final ElementName SUM = new ElementName("sum", "sum", TreeBuilder.OTHER);
    public static final ElementName SIN = new ElementName("sin", "sin", TreeBuilder.OTHER);
    public static final ElementName SEP = new ElementName("sep", "sep", TreeBuilder.OTHER);
    public static final ElementName SUP = new ElementName("sup", "sup", TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName SET = new ElementName("set", "set", TreeBuilder.OTHER);
    public static final ElementName TAN = new ElementName("tan", "tan", TreeBuilder.OTHER);
    public static final ElementName USE = new ElementName("use", "use", TreeBuilder.OTHER);
    public static final ElementName VAR = new ElementName("var", "var", TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName G = new ElementName("g", "g", TreeBuilder.OTHER);
    public static final ElementName WBR = new ElementName("wbr", "wbr", TreeBuilder.AREA_OR_WBR | SPECIAL);
    public static final ElementName XMP = new ElementName("xmp", "xmp", TreeBuilder.XMP | SPECIAL);
    public static final ElementName XOR = new ElementName("xor", "xor", TreeBuilder.OTHER);
    public static final ElementName I = new ElementName("i", "i", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName P = new ElementName("p", "p", TreeBuilder.P | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName Q = new ElementName("q", "q", TreeBuilder.OTHER);
    public static final ElementName S = new ElementName("s", "s", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName U = new ElementName("u", "u", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName H1 = new ElementName("h1", "h1", TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H2 = new ElementName("h2", "h2", TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H3 = new ElementName("h3", "h3", TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H4 = new ElementName("h4", "h4", TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H5 = new ElementName("h5", "h5", TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H6 = new ElementName("h6", "h6", TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName AREA = new ElementName("area", "area", TreeBuilder.AREA_OR_WBR | SPECIAL);
    public static final ElementName EULERGAMMA = new ElementName("eulergamma", "eulergamma", TreeBuilder.OTHER);
    public static final ElementName FEFUNCA = new ElementName("fefunca", "feFuncA", TreeBuilder.OTHER);
    public static final ElementName LAMBDA = new ElementName("lambda", "lambda", TreeBuilder.OTHER);
    public static final ElementName METADATA = new ElementName("metadata", "metadata", TreeBuilder.OTHER);
    public static final ElementName META = new ElementName("meta", "meta", TreeBuilder.META | SPECIAL);
    public static final ElementName TEXTAREA = new ElementName("textarea", "textarea", TreeBuilder.TEXTAREA | SPECIAL);
    public static final ElementName FEFUNCB = new ElementName("fefuncb", "feFuncB", TreeBuilder.OTHER);
    public static final ElementName MSUB = new ElementName("msub", "msub", TreeBuilder.OTHER);
    public static final ElementName RB = new ElementName("rb", "rb", TreeBuilder.RB_OR_RTC | OPTIONAL_END_TAG);
    public static final ElementName ARCSEC = new ElementName("arcsec", "arcsec", TreeBuilder.OTHER);
    public static final ElementName ARCCSC = new ElementName("arccsc", "arccsc", TreeBuilder.OTHER);
    public static final ElementName DEFINITION_SRC = new ElementName("definition-src", "definition-src", TreeBuilder.OTHER);
    public static final ElementName DESC = new ElementName("desc", "desc", TreeBuilder.FOREIGNOBJECT_OR_DESC | SCOPING_AS_SVG);
    public static final ElementName FONT_FACE_SRC = new ElementName("font-face-src", "font-face-src", TreeBuilder.OTHER);
    public static final ElementName MFRAC = new ElementName("mfrac", "mfrac", TreeBuilder.OTHER);
    public static final ElementName DD = new ElementName("dd", "dd", TreeBuilder.DD_OR_DT | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName BGSOUND = new ElementName("bgsound", "bgsound", TreeBuilder.LINK_OR_BASEFONT_OR_BGSOUND | SPECIAL);
    public static final ElementName CARD = new ElementName("card", "card", TreeBuilder.OTHER);
    public static final ElementName DISCARD = new ElementName("discard", "discard", TreeBuilder.OTHER);
    public static final ElementName EMBED = new ElementName("embed", "embed", TreeBuilder.EMBED | SPECIAL);
    public static final ElementName FEBLEND = new ElementName("feblend", "feBlend", TreeBuilder.OTHER);
    public static final ElementName FEFLOOD = new ElementName("feflood", "feFlood", TreeBuilder.OTHER);
    public static final ElementName GRAD = new ElementName("grad", "grad", TreeBuilder.OTHER);
    public static final ElementName HEAD = new ElementName("head", "head", TreeBuilder.HEAD | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName LEGEND = new ElementName("legend", "legend", TreeBuilder.OTHER);
    public static final ElementName MFENCED = new ElementName("mfenced", "mfenced", TreeBuilder.OTHER);
    public static final ElementName MPADDED = new ElementName("mpadded", "mpadded", TreeBuilder.OTHER);
    public static final ElementName NOEMBED = new ElementName("noembed", "noembed", TreeBuilder.NOEMBED | SPECIAL);
    public static final ElementName TD = new ElementName("td", "td", TreeBuilder.TD_OR_TH | SPECIAL | SCOPING | OPTIONAL_END_TAG);
    public static final ElementName THEAD = new ElementName("thead", "thead", TreeBuilder.TBODY_OR_THEAD_OR_TFOOT | SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
    public static final ElementName ASIDE = new ElementName("aside", "aside", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName ARTICLE = new ElementName("article", "article", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName ANIMATE = new ElementName("animate", "animate", TreeBuilder.OTHER);
    public static final ElementName BASE = new ElementName("base", "base", TreeBuilder.BASE | SPECIAL);
    public static final ElementName BLOCKQUOTE = new ElementName("blockquote", "blockquote", TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
    public static final ElementName CODE = new ElementName("code", "code", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName CIRCLE = new ElementName("circle", "circle", TreeBuilder.OTHER);
    public static final ElementName COLOR_PROFILE = new ElementName("color-profile", "color-profile", TreeBuilder.OTHER);
    public static final ElementName COMPOSE = new ElementName("compose", "compose", TreeBuilder.OTHER);
    public static final ElementName CONJUGATE = new ElementName("conjugate", "conjugate", TreeBuilder.OTHER);
    public static final ElementName CITE = new ElementName("cite", "cite", TreeBuilder.OTHER);
    public static final ElementName DIVERGENCE = new ElementName("divergence", "divergence", TreeBuilder.OTHER);
    public static final ElementName DIVIDE = new ElementName("divide", "divide", TreeBuilder.OTHER);
    public static final ElementName DEGREE = new ElementName("degree", "degree", TreeBuilder.OTHER);
    public static final ElementName DECLARE = new ElementName("declare", "declare", TreeBuilder.OTHER);
    public static final ElementName DATATEMPLATE = new ElementName("datatemplate", "datatemplate", TreeBuilder.OTHER);
    public static final ElementName EXPONENTIALE = new ElementName("exponentiale", "exponentiale", TreeBuilder.OTHER);
    public static final ElementName ELLIPSE = new ElementName("ellipse", "ellipse", TreeBuilder.OTHER);
    public static final ElementName FONT_FACE = new ElementName("font-face", "font-face", TreeBuilder.OTHER);
    public static final ElementName FETURBULENCE = new ElementName("feturbulence", "feTurbulence", TreeBuilder.OTHER);
    public static final ElementName FEMERGENODE = new ElementName("femergenode", "feMergeNode", TreeBuilder.OTHER);
    public static final ElementName FEIMAGE = new ElementName("feimage", "feImage", TreeBuilder.OTHER);
    public static final ElementName FEMERGE = new ElementName("femerge", "feMerge", TreeBuilder.OTHER);
    public static final ElementName FETILE = new ElementName("fetile", "feTile", TreeBuilder.OTHER);
    public static final ElementName FONT_FACE_NAME = new ElementName("font-face-name", "font-face-name", TreeBuilder.OTHER);
    public static final ElementName FRAME = new ElementName("frame", "frame", TreeBuilder.FRAME | SPECIAL);
    public static final ElementName FIGURE = new ElementName("figure", "figure", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName FALSE = new ElementName("false", "false", TreeBuilder.OTHER);
    public static final ElementName FECOMPOSITE = new ElementName("fecomposite", "feComposite", TreeBuilder.OTHER);
    public static final ElementName IMAGE = new ElementName("image", "image", TreeBuilder.IMAGE);
    public static final ElementName IFRAME = new ElementName("iframe", "iframe", TreeBuilder.IFRAME | SPECIAL);
    public static final ElementName INVERSE = new ElementName("inverse", "inverse", TreeBuilder.OTHER);
    public static final ElementName LINE = new ElementName("line", "line", TreeBuilder.OTHER);
    public static final ElementName LOGBASE = new ElementName("logbase", "logbase", TreeBuilder.OTHER);
    public static final ElementName MSPACE = new ElementName("mspace", "mspace", TreeBuilder.OTHER);
    public static final ElementName MODE = new ElementName("mode", "mode", TreeBuilder.OTHER);
    public static final ElementName MARQUEE = new ElementName("marquee", "marquee", TreeBuilder.MARQUEE_OR_APPLET | SPECIAL | SCOPING);
    public static final ElementName MTABLE = new ElementName("mtable", "mtable", TreeBuilder.OTHER);
    public static final ElementName MSTYLE = new ElementName("mstyle", "mstyle", TreeBuilder.OTHER);
    public static final ElementName MENCLOSE = new ElementName("menclose", "menclose", TreeBuilder.OTHER);
    public static final ElementName NONE = new ElementName("none", "none", TreeBuilder.OTHER);
    public static final ElementName OTHERWISE = new ElementName("otherwise", "otherwise", TreeBuilder.OTHER);
    public static final ElementName PIECE = new ElementName("piece", "piece", TreeBuilder.OTHER);
    public static final ElementName POLYLINE = new ElementName("polyline", "polyline", TreeBuilder.OTHER);
    public static final ElementName PICTURE = new ElementName("picture", "picture", TreeBuilder.OTHER);
    public static final ElementName PIECEWISE = new ElementName("piecewise", "piecewise", TreeBuilder.OTHER);
    public static final ElementName RULE = new ElementName("rule", "rule", TreeBuilder.OTHER);
    public static final ElementName SOURCE = new ElementName("source", "source", TreeBuilder.PARAM_OR_SOURCE_OR_TRACK);
    public static final ElementName STRIKE = new ElementName("strike", "strike", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName STYLE = new ElementName("style", "style", TreeBuilder.STYLE | SPECIAL);
    public static final ElementName TABLE = new ElementName("table", "table", TreeBuilder.TABLE | SPECIAL | FOSTER_PARENTING | SCOPING);
    public static final ElementName TITLE = new ElementName("title", "title", TreeBuilder.TITLE | SPECIAL | SCOPING_AS_SVG);
    public static final ElementName TIME = new ElementName("time", "time", TreeBuilder.OTHER);
    public static final ElementName TRANSPOSE = new ElementName("transpose", "transpose", TreeBuilder.OTHER);
    public static final ElementName TEMPLATE = new ElementName("template", "template", TreeBuilder.TEMPLATE | SPECIAL | SCOPING);
    public static final ElementName TRUE = new ElementName("true", "true", TreeBuilder.OTHER);
    public static final ElementName VARIANCE = new ElementName("variance", "variance", TreeBuilder.OTHER);
    public static final ElementName ALTGLYPHDEF = new ElementName("altglyphdef", "altGlyphDef", TreeBuilder.OTHER);
    public static final ElementName DIFF = new ElementName("diff", "diff", TreeBuilder.OTHER);
    public static final ElementName FACTOROF = new ElementName("factorof", "factorof", TreeBuilder.OTHER);
    public static final ElementName GLYPHREF = new ElementName("glyphref", "glyphRef", TreeBuilder.OTHER);
    public static final ElementName PARTIALDIFF = new ElementName("partialdiff", "partialdiff", TreeBuilder.OTHER);
    public static final ElementName SETDIFF = new ElementName("setdiff", "setdiff", TreeBuilder.OTHER);
    public static final ElementName TREF = new ElementName("tref", "tref", TreeBuilder.OTHER);
    public static final ElementName CEILING = new ElementName("ceiling", "ceiling", TreeBuilder.OTHER);
    public static final ElementName DIALOG = new ElementName("dialog", "dialog", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName FEFUNCG = new ElementName("fefuncg", "feFuncG", TreeBuilder.OTHER);
    public static final ElementName FEDIFFUSELIGHTING = new ElementName("fediffuselighting", "feDiffuseLighting", TreeBuilder.OTHER);
    public static final ElementName FESPECULARLIGHTING = new ElementName("fespecularlighting", "feSpecularLighting", TreeBuilder.OTHER);
    public static final ElementName LISTING = new ElementName("listing", "listing", TreeBuilder.PRE_OR_LISTING | SPECIAL);
    public static final ElementName STRONG = new ElementName("strong", "strong", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName ARCSECH = new ElementName("arcsech", "arcsech", TreeBuilder.OTHER);
    public static final ElementName ARCCSCH = new ElementName("arccsch", "arccsch", TreeBuilder.OTHER);
    public static final ElementName ARCTANH = new ElementName("arctanh", "arctanh", TreeBuilder.OTHER);
    public static final ElementName ARCSINH = new ElementName("arcsinh", "arcsinh", TreeBuilder.OTHER);
    public static final ElementName ALTGLYPH = new ElementName("altglyph", "altGlyph", TreeBuilder.OTHER);
    public static final ElementName ARCCOSH = new ElementName("arccosh", "arccosh", TreeBuilder.OTHER);
    public static final ElementName ARCCOTH = new ElementName("arccoth", "arccoth", TreeBuilder.OTHER);
    public static final ElementName CSCH = new ElementName("csch", "csch", TreeBuilder.OTHER);
    public static final ElementName COSH = new ElementName("cosh", "cosh", TreeBuilder.OTHER);
    public static final ElementName CLIPPATH = new ElementName("clippath", "clipPath", TreeBuilder.OTHER);
    public static final ElementName COTH = new ElementName("coth", "coth", TreeBuilder.OTHER);
    public static final ElementName GLYPH = new ElementName("glyph", "glyph", TreeBuilder.OTHER);
    public static final ElementName MGLYPH = new ElementName("mglyph", "mglyph", TreeBuilder.MGLYPH_OR_MALIGNMARK);
    public static final ElementName MISSING_GLYPH = new ElementName("missing-glyph", "missing-glyph", TreeBuilder.OTHER);
    public static final ElementName MATH = new ElementName("math", "math", TreeBuilder.MATH);
    public static final ElementName MPATH = new ElementName("mpath", "mpath", TreeBuilder.OTHER);
    public static final ElementName PREFETCH = new ElementName("prefetch", "prefetch", TreeBuilder.OTHER);
    public static final ElementName PATH = new ElementName("path", "path", TreeBuilder.OTHER);
    public static final ElementName TH = new ElementName("th", "th", TreeBuilder.TD_OR_TH | SPECIAL | SCOPING | OPTIONAL_END_TAG);
    public static final ElementName SECH = new ElementName("sech", "sech", TreeBuilder.OTHER);
    public static final ElementName SWITCH = new ElementName("switch", "switch", TreeBuilder.OTHER);
    public static final ElementName SINH = new ElementName("sinh", "sinh", TreeBuilder.OTHER);
    public static final ElementName TANH = new ElementName("tanh", "tanh", TreeBuilder.OTHER);
    public static final ElementName TEXTPATH = new ElementName("textpath", "textPath", TreeBuilder.OTHER);
    public static final ElementName CI = new ElementName("ci", "ci", TreeBuilder.OTHER);
    public static final ElementName FONT_FACE_URI = new ElementName("font-face-uri", "font-face-uri", TreeBuilder.OTHER);
    public static final ElementName LI = new ElementName("li", "li", TreeBuilder.LI | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName IMAGINARYI = new ElementName("imaginaryi", "imaginaryi", TreeBuilder.OTHER);
    public static final ElementName MI = new ElementName("mi", "mi", TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName PI = new ElementName("pi", "pi", TreeBuilder.OTHER);
    public static final ElementName LINK = new ElementName("link", "link", TreeBuilder.LINK_OR_BASEFONT_OR_BGSOUND | SPECIAL);
    public static final ElementName MARK = new ElementName("mark", "mark", TreeBuilder.OTHER);
    public static final ElementName MALIGNMARK = new ElementName("malignmark", "malignmark", TreeBuilder.MGLYPH_OR_MALIGNMARK);
    public static final ElementName MASK = new ElementName("mask", "mask", TreeBuilder.OTHER);
    public static final ElementName TBREAK = new ElementName("tbreak", "tbreak", TreeBuilder.OTHER);
    public static final ElementName TRACK = new ElementName("track", "track", TreeBuilder.PARAM_OR_SOURCE_OR_TRACK | SPECIAL);
    public static final ElementName DL = new ElementName("dl", "dl", TreeBuilder.UL_OR_OL_OR_DL | SPECIAL);
    public static final ElementName ANNOTATION_XML = new ElementName("annotation-xml", "annotation-xml", TreeBuilder.ANNOTATION_XML | SCOPING_AS_MATHML);
    public static final ElementName CSYMBOL = new ElementName("csymbol", "csymbol", TreeBuilder.OTHER);
    public static final ElementName CURL = new ElementName("curl", "curl", TreeBuilder.OTHER);
    public static final ElementName FACTORIAL = new ElementName("factorial", "factorial", TreeBuilder.OTHER);
    public static final ElementName FORALL = new ElementName("forall", "forall", TreeBuilder.OTHER);
    public static final ElementName HTML = new ElementName("html", "html", TreeBuilder.HTML | SPECIAL | SCOPING | OPTIONAL_END_TAG);
    public static final ElementName INTERVAL = new ElementName("interval", "interval", TreeBuilder.OTHER);
    public static final ElementName OL = new ElementName("ol", "ol", TreeBuilder.UL_OR_OL_OR_DL | SPECIAL);
    public static final ElementName LABEL = new ElementName("label", "label", TreeBuilder.OTHER);
    public static final ElementName UL = new ElementName("ul", "ul", TreeBuilder.UL_OR_OL_OR_DL | SPECIAL);
    public static final ElementName REAL = new ElementName("real", "real", TreeBuilder.OTHER);
    public static final ElementName SMALL = new ElementName("small", "small", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName SYMBOL = new ElementName("symbol", "symbol", TreeBuilder.OTHER);
    public static final ElementName ALTGLYPHITEM = new ElementName("altglyphitem", "altGlyphItem", TreeBuilder.OTHER);
    public static final ElementName ANIMATETRANSFORM = new ElementName("animatetransform", "animateTransform", TreeBuilder.OTHER);
    public static final ElementName ACRONYM = new ElementName("acronym", "acronym", TreeBuilder.OTHER);
    public static final ElementName EM = new ElementName("em", "em", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName FORM = new ElementName("form", "form", TreeBuilder.FORM | SPECIAL);
    public static final ElementName MENUITEM = new ElementName("menuitem", "menuitem", TreeBuilder.MENUITEM);
    public static final ElementName MPHANTOM = new ElementName("mphantom", "mphantom", TreeBuilder.OTHER);
    public static final ElementName PARAM = new ElementName("param", "param", TreeBuilder.PARAM_OR_SOURCE_OR_TRACK | SPECIAL);
    public static final ElementName CN = new ElementName("cn", "cn", TreeBuilder.OTHER);
    public static final ElementName ARCTAN = new ElementName("arctan", "arctan", TreeBuilder.OTHER);
    public static final ElementName ARCSIN = new ElementName("arcsin", "arcsin", TreeBuilder.OTHER);
    public static final ElementName ANIMATION = new ElementName("animation", "animation", TreeBuilder.OTHER);
    public static final ElementName ANNOTATION = new ElementName("annotation", "annotation", TreeBuilder.OTHER);
    public static final ElementName ANIMATEMOTION = new ElementName("animatemotion", "animateMotion", TreeBuilder.OTHER);
    public static final ElementName BUTTON = new ElementName("button", "button", TreeBuilder.BUTTON | SPECIAL);
    public static final ElementName FN = new ElementName("fn", "fn", TreeBuilder.OTHER);
    public static final ElementName CODOMAIN = new ElementName("codomain", "codomain", TreeBuilder.OTHER);
    public static final ElementName CAPTION = new ElementName("caption", "caption", TreeBuilder.CAPTION | SPECIAL | SCOPING);
    public static final ElementName CONDITION = new ElementName("condition", "condition", TreeBuilder.OTHER);
    public static final ElementName DOMAIN = new ElementName("domain", "domain", TreeBuilder.OTHER);
    public static final ElementName DOMAINOFAPPLICATION = new ElementName("domainofapplication", "domainofapplication", TreeBuilder.OTHER);
    public static final ElementName IN = new ElementName("in", "in", TreeBuilder.OTHER);
    public static final ElementName FIGCAPTION = new ElementName("figcaption", "figcaption", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName HKERN = new ElementName("hkern", "hkern", TreeBuilder.OTHER);
    public static final ElementName LN = new ElementName("ln", "ln", TreeBuilder.OTHER);
    public static final ElementName MN = new ElementName("mn", "mn", TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName KEYGEN = new ElementName("keygen", "keygen", TreeBuilder.KEYGEN);
    public static final ElementName LAPLACIAN = new ElementName("laplacian", "laplacian", TreeBuilder.OTHER);
    public static final ElementName MEAN = new ElementName("mean", "mean", TreeBuilder.OTHER);
    public static final ElementName MEDIAN = new ElementName("median", "median", TreeBuilder.OTHER);
    public static final ElementName MAIN = new ElementName("main", "main", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName MACTION = new ElementName("maction", "maction", TreeBuilder.OTHER);
    public static final ElementName NOTIN = new ElementName("notin", "notin", TreeBuilder.OTHER);
    public static final ElementName OPTION = new ElementName("option", "option", TreeBuilder.OPTION | OPTIONAL_END_TAG);
    public static final ElementName POLYGON = new ElementName("polygon", "polygon", TreeBuilder.OTHER);
    public static final ElementName PATTERN = new ElementName("pattern", "pattern", TreeBuilder.OTHER);
    public static final ElementName RELN = new ElementName("reln", "reln", TreeBuilder.OTHER);
    public static final ElementName SPAN = new ElementName("span", "span", TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName SECTION = new ElementName("section", "section", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName TSPAN = new ElementName("tspan", "tspan", TreeBuilder.OTHER);
    public static final ElementName UNION = new ElementName("union", "union", TreeBuilder.OTHER);
    public static final ElementName VKERN = new ElementName("vkern", "vkern", TreeBuilder.OTHER);
    public static final ElementName AUDIO = new ElementName("audio", "audio", TreeBuilder.OTHER);
    public static final ElementName MO = new ElementName("mo", "mo", TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName TENDSTO = new ElementName("tendsto", "tendsto", TreeBuilder.OTHER);
    public static final ElementName VIDEO = new ElementName("video", "video", TreeBuilder.OTHER);
    public static final ElementName COLGROUP = new ElementName("colgroup", "colgroup", TreeBuilder.COLGROUP | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName FEDISPLACEMENTMAP = new ElementName("fedisplacementmap", "feDisplacementMap", TreeBuilder.OTHER);
    public static final ElementName HGROUP = new ElementName("hgroup", "hgroup", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName MALIGNGROUP = new ElementName("maligngroup", "maligngroup", TreeBuilder.OTHER);
    public static final ElementName MSUBSUP = new ElementName("msubsup", "msubsup", TreeBuilder.OTHER);
    public static final ElementName MSUP = new ElementName("msup", "msup", TreeBuilder.OTHER);
    public static final ElementName RP = new ElementName("rp", "rp", TreeBuilder.RT_OR_RP | OPTIONAL_END_TAG);
    public static final ElementName OPTGROUP = new ElementName("optgroup", "optgroup", TreeBuilder.OPTGROUP | OPTIONAL_END_TAG);
    public static final ElementName SAMP = new ElementName("samp", "samp", TreeBuilder.OTHER);
    public static final ElementName STOP = new ElementName("stop", "stop", TreeBuilder.OTHER);
    public static final ElementName EQ = new ElementName("eq", "eq", TreeBuilder.OTHER);
    public static final ElementName BR = new ElementName("br", "br", TreeBuilder.BR | SPECIAL);
    public static final ElementName ABBR = new ElementName("abbr", "abbr", TreeBuilder.OTHER);
    public static final ElementName ANIMATECOLOR = new ElementName("animatecolor", "animateColor", TreeBuilder.OTHER);
    public static final ElementName BVAR = new ElementName("bvar", "bvar", TreeBuilder.OTHER);
    public static final ElementName CENTER = new ElementName("center", "center", TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
    public static final ElementName CURSOR = new ElementName("cursor", "cursor", TreeBuilder.OTHER);
    public static final ElementName HR = new ElementName("hr", "hr", TreeBuilder.HR | SPECIAL);
    public static final ElementName FEFUNCR = new ElementName("fefuncr", "feFuncR", TreeBuilder.OTHER);
    public static final ElementName FECOMPONENTTRANSFER = new ElementName("fecomponenttransfer", "feComponentTransfer", TreeBuilder.OTHER);
    public static final ElementName FILTER = new ElementName("filter", "filter", TreeBuilder.OTHER);
    public static final ElementName FOOTER = new ElementName("footer", "footer", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName FLOOR = new ElementName("floor", "floor", TreeBuilder.OTHER);
    public static final ElementName FEGAUSSIANBLUR = new ElementName("fegaussianblur", "feGaussianBlur", TreeBuilder.OTHER);
    public static final ElementName HEADER = new ElementName("header", "header", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName HANDLER = new ElementName("handler", "handler", TreeBuilder.OTHER);
    public static final ElementName OR = new ElementName("or", "or", TreeBuilder.OTHER);
    public static final ElementName LISTENER = new ElementName("listener", "listener", TreeBuilder.OTHER);
    public static final ElementName MUNDER = new ElementName("munder", "munder", TreeBuilder.OTHER);
    public static final ElementName MARKER = new ElementName("marker", "marker", TreeBuilder.OTHER);
    public static final ElementName METER = new ElementName("meter", "meter", TreeBuilder.OTHER);
    public static final ElementName MOVER = new ElementName("mover", "mover", TreeBuilder.OTHER);
    public static final ElementName MUNDEROVER = new ElementName("munderover", "munderover", TreeBuilder.OTHER);
    public static final ElementName MERROR = new ElementName("merror", "merror", TreeBuilder.OTHER);
    public static final ElementName MLABELEDTR = new ElementName("mlabeledtr", "mlabeledtr", TreeBuilder.OTHER);
    public static final ElementName NOBR = new ElementName("nobr", "nobr", TreeBuilder.NOBR);
    public static final ElementName NOTANUMBER = new ElementName("notanumber", "notanumber", TreeBuilder.OTHER);
    public static final ElementName POWER = new ElementName("power", "power", TreeBuilder.OTHER);
    public static final ElementName TR = new ElementName("tr", "tr", TreeBuilder.TR | SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
    public static final ElementName SOLIDCOLOR = new ElementName("solidcolor", "solidcolor", TreeBuilder.OTHER);
    public static final ElementName SELECTOR = new ElementName("selector", "selector", TreeBuilder.OTHER);
    public static final ElementName VECTOR = new ElementName("vector", "vector", TreeBuilder.OTHER);
    public static final ElementName ARCCOS = new ElementName("arccos", "arccos", TreeBuilder.OTHER);
    public static final ElementName ADDRESS = new ElementName("address", "address", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName CANVAS = new ElementName("canvas", "canvas", TreeBuilder.OTHER);
    public static final ElementName COMPLEXES = new ElementName("complexes", "complexes", TreeBuilder.OTHER);
    public static final ElementName DEFS = new ElementName("defs", "defs", TreeBuilder.OTHER);
    public static final ElementName DETAILS = new ElementName("details", "details", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName EXISTS = new ElementName("exists", "exists", TreeBuilder.OTHER);
    public static final ElementName IMPLIES = new ElementName("implies", "implies", TreeBuilder.OTHER);
    public static final ElementName INTEGERS = new ElementName("integers", "integers", TreeBuilder.OTHER);
    public static final ElementName MS = new ElementName("ms", "ms", TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName MPRESCRIPTS = new ElementName("mprescripts", "mprescripts", TreeBuilder.OTHER);
    public static final ElementName MMULTISCRIPTS = new ElementName("mmultiscripts", "mmultiscripts", TreeBuilder.OTHER);
    public static final ElementName MINUS = new ElementName("minus", "minus", TreeBuilder.OTHER);
    public static final ElementName NOFRAMES = new ElementName("noframes", "noframes", TreeBuilder.NOFRAMES | SPECIAL);
    public static final ElementName NATURALNUMBERS = new ElementName("naturalnumbers", "naturalnumbers", TreeBuilder.OTHER);
    public static final ElementName PRIMES = new ElementName("primes", "primes", TreeBuilder.OTHER);
    public static final ElementName PROGRESS = new ElementName("progress", "progress", TreeBuilder.OTHER);
    public static final ElementName PLUS = new ElementName("plus", "plus", TreeBuilder.OTHER);
    public static final ElementName REALS = new ElementName("reals", "reals", TreeBuilder.OTHER);
    public static final ElementName RATIONALS = new ElementName("rationals", "rationals", TreeBuilder.OTHER);
    public static final ElementName SEMANTICS = new ElementName("semantics", "semantics", TreeBuilder.OTHER);
    public static final ElementName TIMES = new ElementName("times", "times", TreeBuilder.OTHER);
    public static final ElementName DT = new ElementName("dt", "dt", TreeBuilder.DD_OR_DT | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName APPLET = new ElementName("applet", "applet", TreeBuilder.MARQUEE_OR_APPLET | SPECIAL | SCOPING);
    public static final ElementName ARCCOT = new ElementName("arccot", "arccot", TreeBuilder.OTHER);
    public static final ElementName BASEFONT = new ElementName("basefont", "basefont", TreeBuilder.LINK_OR_BASEFONT_OR_BGSOUND | SPECIAL);
    public static final ElementName CARTESIANPRODUCT = new ElementName("cartesianproduct", "cartesianproduct", TreeBuilder.OTHER);
    public static final ElementName GT = new ElementName("gt", "gt", TreeBuilder.OTHER);
    public static final ElementName DETERMINANT = new ElementName("determinant", "determinant", TreeBuilder.OTHER);
    public static final ElementName EMPTYSET = new ElementName("emptyset", "emptyset", TreeBuilder.OTHER);
    public static final ElementName EQUIVALENT = new ElementName("equivalent", "equivalent", TreeBuilder.OTHER);
    public static final ElementName FONT_FACE_FORMAT = new ElementName("font-face-format", "font-face-format", TreeBuilder.OTHER);
    public static final ElementName FOREIGNOBJECT = new ElementName("foreignobject", "foreignObject", TreeBuilder.FOREIGNOBJECT_OR_DESC | SCOPING_AS_SVG);
    public static final ElementName FIELDSET = new ElementName("fieldset", "fieldset", TreeBuilder.FIELDSET | SPECIAL);
    public static final ElementName FRAMESET = new ElementName("frameset", "frameset", TreeBuilder.FRAMESET | SPECIAL);
    public static final ElementName FEOFFSET = new ElementName("feoffset", "feOffset", TreeBuilder.OTHER);
    public static final ElementName FESPOTLIGHT = new ElementName("fespotlight", "feSpotLight", TreeBuilder.OTHER);
    public static final ElementName FEPOINTLIGHT = new ElementName("fepointlight", "fePointLight", TreeBuilder.OTHER);
    public static final ElementName FEDISTANTLIGHT = new ElementName("fedistantlight", "feDistantLight", TreeBuilder.OTHER);
    public static final ElementName FONT = new ElementName("font", "font", TreeBuilder.FONT);
    public static final ElementName LT = new ElementName("lt", "lt", TreeBuilder.OTHER);
    public static final ElementName INTERSECT = new ElementName("intersect", "intersect", TreeBuilder.OTHER);
    public static final ElementName IDENT = new ElementName("ident", "ident", TreeBuilder.OTHER);
    public static final ElementName INPUT = new ElementName("input", "input", TreeBuilder.INPUT | SPECIAL);
    public static final ElementName LIMIT = new ElementName("limit", "limit", TreeBuilder.OTHER);
    public static final ElementName LOWLIMIT = new ElementName("lowlimit", "lowlimit", TreeBuilder.OTHER);
    public static final ElementName LINEARGRADIENT = new ElementName("lineargradient", "linearGradient", TreeBuilder.OTHER);
    public static final ElementName LIST = new ElementName("list", "list", TreeBuilder.OTHER);
    public static final ElementName MOMENT = new ElementName("moment", "moment", TreeBuilder.OTHER);
    public static final ElementName MROOT = new ElementName("mroot", "mroot", TreeBuilder.OTHER);
    public static final ElementName MSQRT = new ElementName("msqrt", "msqrt", TreeBuilder.OTHER);
    public static final ElementName MOMENTABOUT = new ElementName("momentabout", "momentabout", TreeBuilder.OTHER);
    public static final ElementName MTEXT = new ElementName("mtext", "mtext", TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName NOTSUBSET = new ElementName("notsubset", "notsubset", TreeBuilder.OTHER);
    public static final ElementName NOTPRSUBSET = new ElementName("notprsubset", "notprsubset", TreeBuilder.OTHER);
    public static final ElementName NOSCRIPT = new ElementName("noscript", "noscript", TreeBuilder.NOSCRIPT | SPECIAL);
    public static final ElementName NEST = new ElementName("nest", "nest", TreeBuilder.OTHER);
    public static final ElementName RT = new ElementName("rt", "rt", TreeBuilder.RT_OR_RP | OPTIONAL_END_TAG);
    public static final ElementName OBJECT = new ElementName("object", "object", TreeBuilder.OBJECT | SPECIAL | SCOPING);
    public static final ElementName OUTERPRODUCT = new ElementName("outerproduct", "outerproduct", TreeBuilder.OTHER);
    public static final ElementName OUTPUT = new ElementName("output", "output", TreeBuilder.OUTPUT);
    public static final ElementName PRODUCT = new ElementName("product", "product", TreeBuilder.OTHER);
    public static final ElementName PRSUBSET = new ElementName("prsubset", "prsubset", TreeBuilder.OTHER);
    public static final ElementName PLAINTEXT = new ElementName("plaintext", "plaintext", TreeBuilder.PLAINTEXT | SPECIAL);
    public static final ElementName TT = new ElementName("tt", "tt", TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName QUOTIENT = new ElementName("quotient", "quotient", TreeBuilder.OTHER);
    public static final ElementName RECT = new ElementName("rect", "rect", TreeBuilder.OTHER);
    public static final ElementName RADIALGRADIENT = new ElementName("radialgradient", "radialGradient", TreeBuilder.OTHER);
    public static final ElementName ROOT = new ElementName("root", "root", TreeBuilder.OTHER);
    public static final ElementName SELECT = new ElementName("select", "select", TreeBuilder.SELECT | SPECIAL);
    public static final ElementName SCALARPRODUCT = new ElementName("scalarproduct", "scalarproduct", TreeBuilder.OTHER);
    public static final ElementName SUBSET = new ElementName("subset", "subset", TreeBuilder.OTHER);
    public static final ElementName SCRIPT = new ElementName("script", "script", TreeBuilder.SCRIPT | SPECIAL);
    public static final ElementName TFOOT = new ElementName("tfoot", "tfoot", TreeBuilder.TBODY_OR_THEAD_OR_TFOOT | SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
    public static final ElementName TEXT = new ElementName("text", "text", TreeBuilder.OTHER);
    public static final ElementName UPLIMIT = new ElementName("uplimit", "uplimit", TreeBuilder.OTHER);
    public static final ElementName VECTORPRODUCT = new ElementName("vectorproduct", "vectorproduct", TreeBuilder.OTHER);
    public static final ElementName MENU = new ElementName("menu", "menu", TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
    public static final ElementName SDEV = new ElementName("sdev", "sdev", TreeBuilder.OTHER);
    public static final ElementName FEDROPSHADOW = new ElementName("fedropshadow", "feDropShadow", TreeBuilder.OTHER);
    public static final ElementName MROW = new ElementName("mrow", "mrow", TreeBuilder.OTHER);
    public static final ElementName MATRIXROW = new ElementName("matrixrow", "matrixrow", TreeBuilder.OTHER);
    public static final ElementName VIEW = new ElementName("view", "view", TreeBuilder.OTHER);
    public static final ElementName APPROX = new ElementName("approx", "approx", TreeBuilder.OTHER);
    public static final ElementName FECOLORMATRIX = new ElementName("fecolormatrix", "feColorMatrix", TreeBuilder.OTHER);
    public static final ElementName FECONVOLVEMATRIX = new ElementName("feconvolvematrix", "feConvolveMatrix", TreeBuilder.OTHER);
    public static final ElementName ISINDEX = new ElementName("isindex", "isindex", TreeBuilder.ISINDEX | SPECIAL);
    public static final ElementName MATRIX = new ElementName("matrix", "matrix", TreeBuilder.OTHER);
    public static final ElementName APPLY = new ElementName("apply", "apply", TreeBuilder.OTHER);
    public static final ElementName BODY = new ElementName("body", "body", TreeBuilder.BODY | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName FEMORPHOLOGY = new ElementName("femorphology", "feMorphology", TreeBuilder.OTHER);
    public static final ElementName IMAGINARY = new ElementName("imaginary", "imaginary", TreeBuilder.OTHER);
    public static final ElementName INFINITY = new ElementName("infinity", "infinity", TreeBuilder.OTHER);
    public static final ElementName RUBY = new ElementName("ruby", "ruby", TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName SUMMARY = new ElementName("summary", "summary", TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName TBODY = new ElementName("tbody", "tbody", TreeBuilder.TBODY_OR_THEAD_OR_TFOOT | SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
    private final static @NoLength ElementName[] ELEMENT_NAMES = {
    AND,
    ARG,
    ABS,
    BIG,
    BDO,
    CSC,
    COL,
    COS,
    COT,
    DEL,
    DFN,
    DIR,
    DIV,
    EXP,
    GCD,
    GEQ,
    IMG,
    INS,
    INT,
    KBD,
    LOG,
    LCM,
    LEQ,
    MTD,
    MIN,
    MAP,
    MTR,
    MAX,
    NEQ,
    NOT,
    NAV,
    PRE,
    A,
    B,
    RTC,
    REM,
    SUB,
    SEC,
    SVG,
    SUM,
    SIN,
    SEP,
    SUP,
    SET,
    TAN,
    USE,
    VAR,
    G,
    WBR,
    XMP,
    XOR,
    I,
    P,
    Q,
    S,
    U,
    H1,
    H2,
    H3,
    H4,
    H5,
    H6,
    AREA,
    EULERGAMMA,
    FEFUNCA,
    LAMBDA,
    METADATA,
    META,
    TEXTAREA,
    FEFUNCB,
    MSUB,
    RB,
    ARCSEC,
    ARCCSC,
    DEFINITION_SRC,
    DESC,
    FONT_FACE_SRC,
    MFRAC,
    DD,
    BGSOUND,
    CARD,
    DISCARD,
    EMBED,
    FEBLEND,
    FEFLOOD,
    GRAD,
    HEAD,
    LEGEND,
    MFENCED,
    MPADDED,
    NOEMBED,
    TD,
    THEAD,
    ASIDE,
    ARTICLE,
    ANIMATE,
    BASE,
    BLOCKQUOTE,
    CODE,
    CIRCLE,
    COLOR_PROFILE,
    COMPOSE,
    CONJUGATE,
    CITE,
    DIVERGENCE,
    DIVIDE,
    DEGREE,
    DECLARE,
    DATATEMPLATE,
    EXPONENTIALE,
    ELLIPSE,
    FONT_FACE,
    FETURBULENCE,
    FEMERGENODE,
    FEIMAGE,
    FEMERGE,
    FETILE,
    FONT_FACE_NAME,
    FRAME,
    FIGURE,
    FALSE,
    FECOMPOSITE,
    IMAGE,
    IFRAME,
    INVERSE,
    LINE,
    LOGBASE,
    MSPACE,
    MODE,
    MARQUEE,
    MTABLE,
    MSTYLE,
    MENCLOSE,
    NONE,
    OTHERWISE,
    PIECE,
    POLYLINE,
    PICTURE,
    PIECEWISE,
    RULE,
    SOURCE,
    STRIKE,
    STYLE,
    TABLE,
    TITLE,
    TIME,
    TRANSPOSE,
    TEMPLATE,
    TRUE,
    VARIANCE,
    ALTGLYPHDEF,
    DIFF,
    FACTOROF,
    GLYPHREF,
    PARTIALDIFF,
    SETDIFF,
    TREF,
    CEILING,
    DIALOG,
    FEFUNCG,
    FEDIFFUSELIGHTING,
    FESPECULARLIGHTING,
    LISTING,
    STRONG,
    ARCSECH,
    ARCCSCH,
    ARCTANH,
    ARCSINH,
    ALTGLYPH,
    ARCCOSH,
    ARCCOTH,
    CSCH,
    COSH,
    CLIPPATH,
    COTH,
    GLYPH,
    MGLYPH,
    MISSING_GLYPH,
    MATH,
    MPATH,
    PREFETCH,
    PATH,
    TH,
    SECH,
    SWITCH,
    SINH,
    TANH,
    TEXTPATH,
    CI,
    FONT_FACE_URI,
    LI,
    IMAGINARYI,
    MI,
    PI,
    LINK,
    MARK,
    MALIGNMARK,
    MASK,
    TBREAK,
    TRACK,
    DL,
    ANNOTATION_XML,
    CSYMBOL,
    CURL,
    FACTORIAL,
    FORALL,
    HTML,
    INTERVAL,
    OL,
    LABEL,
    UL,
    REAL,
    SMALL,
    SYMBOL,
    ALTGLYPHITEM,
    ANIMATETRANSFORM,
    ACRONYM,
    EM,
    FORM,
    MENUITEM,
    MPHANTOM,
    PARAM,
    CN,
    ARCTAN,
    ARCSIN,
    ANIMATION,
    ANNOTATION,
    ANIMATEMOTION,
    BUTTON,
    FN,
    CODOMAIN,
    CAPTION,
    CONDITION,
    DOMAIN,
    DOMAINOFAPPLICATION,
    IN,
    FIGCAPTION,
    HKERN,
    LN,
    MN,
    KEYGEN,
    LAPLACIAN,
    MEAN,
    MEDIAN,
    MAIN,
    MACTION,
    NOTIN,
    OPTION,
    POLYGON,
    PATTERN,
    RELN,
    SPAN,
    SECTION,
    TSPAN,
    UNION,
    VKERN,
    AUDIO,
    MO,
    TENDSTO,
    VIDEO,
    COLGROUP,
    FEDISPLACEMENTMAP,
    HGROUP,
    MALIGNGROUP,
    MSUBSUP,
    MSUP,
    RP,
    OPTGROUP,
    SAMP,
    STOP,
    EQ,
    BR,
    ABBR,
    ANIMATECOLOR,
    BVAR,
    CENTER,
    CURSOR,
    HR,
    FEFUNCR,
    FECOMPONENTTRANSFER,
    FILTER,
    FOOTER,
    FLOOR,
    FEGAUSSIANBLUR,
    HEADER,
    HANDLER,
    OR,
    LISTENER,
    MUNDER,
    MARKER,
    METER,
    MOVER,
    MUNDEROVER,
    MERROR,
    MLABELEDTR,
    NOBR,
    NOTANUMBER,
    POWER,
    TR,
    SOLIDCOLOR,
    SELECTOR,
    VECTOR,
    ARCCOS,
    ADDRESS,
    CANVAS,
    COMPLEXES,
    DEFS,
    DETAILS,
    EXISTS,
    IMPLIES,
    INTEGERS,
    MS,
    MPRESCRIPTS,
    MMULTISCRIPTS,
    MINUS,
    NOFRAMES,
    NATURALNUMBERS,
    PRIMES,
    PROGRESS,
    PLUS,
    REALS,
    RATIONALS,
    SEMANTICS,
    TIMES,
    DT,
    APPLET,
    ARCCOT,
    BASEFONT,
    CARTESIANPRODUCT,
    GT,
    DETERMINANT,
    EMPTYSET,
    EQUIVALENT,
    FONT_FACE_FORMAT,
    FOREIGNOBJECT,
    FIELDSET,
    FRAMESET,
    FEOFFSET,
    FESPOTLIGHT,
    FEPOINTLIGHT,
    FEDISTANTLIGHT,
    FONT,
    LT,
    INTERSECT,
    IDENT,
    INPUT,
    LIMIT,
    LOWLIMIT,
    LINEARGRADIENT,
    LIST,
    MOMENT,
    MROOT,
    MSQRT,
    MOMENTABOUT,
    MTEXT,
    NOTSUBSET,
    NOTPRSUBSET,
    NOSCRIPT,
    NEST,
    RT,
    OBJECT,
    OUTERPRODUCT,
    OUTPUT,
    PRODUCT,
    PRSUBSET,
    PLAINTEXT,
    TT,
    QUOTIENT,
    RECT,
    RADIALGRADIENT,
    ROOT,
    SELECT,
    SCALARPRODUCT,
    SUBSET,
    SCRIPT,
    TFOOT,
    TEXT,
    UPLIMIT,
    VECTORPRODUCT,
    MENU,
    SDEV,
    FEDROPSHADOW,
    MROW,
    MATRIXROW,
    VIEW,
    APPROX,
    FECOLORMATRIX,
    FECONVOLVEMATRIX,
    ISINDEX,
    MATRIX,
    APPLY,
    BODY,
    FEMORPHOLOGY,
    IMAGINARY,
    INFINITY,
    RUBY,
    SUMMARY,
    TBODY,
    };
    private final static int[] ELEMENT_HASHES = {
    50908899,
    50910499,
    50916387,
    51434643,
    51438659,
    51957043,
    51961587,
    51965171,
    51965683,
    52485715,
    52486755,
    52488851,
    52490899,
    53012355,
    54054451,
    54061139,
    55104723,
    55110883,
    55111395,
    56151587,
    56677619,
    56680499,
    56682579,
    57200451,
    57205395,
    57206291,
    57207619,
    57210387,
    57731155,
    57732851,
    57733651,
    58773795,
    59244545,
    59768833,
    59821379,
    59826259,
    60345171,
    60345427,
    60347747,
    60350803,
    60351123,
    60352083,
    60352339,
    60354131,
    60875283,
    61395251,
    61925907,
    62390273,
    62450211,
    62973651,
    62974707,
    63438849,
    67108865,
    67633153,
    68681729,
    69730305,
    876609538,
    893386754,
    910163970,
    926941186,
    943718402,
    960495618,
    1679960596,
    1682186266,
    1682547543,
    1685703382,
    1686489160,
    1686491348,
    1689922072,
    1699324759,
    1703292116,
    1703936002,
    1713515574,
    1713736758,
    1715300574,
    1715310660,
    1716349149,
    1719741029,
    1730150402,
    1730965751,
    1731545140,
    1732069431,
    1732381397,
    1733054663,
    1733076167,
    1733372532,
    1733890180,
    1736200310,
    1736576231,
    1736576583,
    1737099991,
    1738539010,
    1740181637,
    1747048757,
    1747176599,
    1747306711,
    1747814436,
    1747838298,
    1748100148,
    1748225318,
    1748228205,
    1748346119,
    1748355193,
    1748359220,
    1748607578,
    1748621670,
    1748642422,
    1748846791,
    1748879564,
    1749272732,
    1749395095,
    1749649513,
    1749656156,
    1749673195,
    1749715159,
    1749723735,
    1749801286,
    1749813486,
    1749813541,
    1749905526,
    1749917205,
    1749932347,
    1751288021,
    1751386406,
    1751493207,
    1752979652,
    1753057319,
    1753319686,
    1753343188,
    1753362711,
    1753467414,
    1753479494,
    1753588936,
    1754031332,
    1754634617,
    1754894485,
    1755076808,
    1755148615,
    1755158905,
    1756098852,
    1756474198,
    1756600614,
    1756625221,
    1757137429,
    1757146773,
    1757157700,
    1757259017,
    1757268168,
    1757293380,
    1758044696,
    1763839627,
    1765431364,
    1766632184,
    1766992520,
    1771722827,
    1773295687,
    1773808452,
    1781815495,
    1782357526,
    1783210839,
    1783388497,
    1783388498,
    1786534215,
    1790207270,
    1797361975,
    1797368887,
    1797540167,
    1797544247,
    1797585096,
    1797628983,
    1797645367,
    1798417460,
    1798677556,
    1798686984,
    1798693940,
    1800730821,
    1803876550,
    1803876557,
    1803929812,
    1803929861,
    1805233752,
    1805502724,
    1805647874,
    1806799156,
    1806806678,
    1806981428,
    1807501636,
    1807599880,
    1813512194,
    1817013469,
    1818230786,
    1818700314,
    1818755074,
    1820327938,
    1853642948,
    1854228692,
    1854228698,
    1854245076,
    1857622310,
    1857653029,
    1864368130,
    1864643294,
    1865714391,
    1865773108,
    1867061545,
    1867237670,
    1868312196,
    1868641064,
    1870135298,
    1870268949,
    1873281026,
    1873350948,
    1874053333,
    1874102998,
    1881288348,
    1881498736,
    1881613047,
    1881669634,
    1884120164,
    1887579800,
    1887743720,
    1889085973,
    1897398274,
    1897999926,
    1898130486,
    1898223945,
    1898223946,
    1898223949,
    1898753862,
    1898971138,
    1899170008,
    1899272519,
    1899272521,
    1899694294,
    1899796819,
    1900544002,
    1900845386,
    1901940917,
    1902116866,
    1902641154,
    1903302038,
    1903761465,
    1904283860,
    1904285766,
    1904412884,
    1904515399,
    1904946933,
    1905563974,
    1906087319,
    1906135367,
    1907085604,
    1907435316,
    1907661127,
    1907959605,
    1908709605,
    1909280949,
    1914900309,
    1919418370,
    1925049415,
    1925844629,
    1932928296,
    1934172497,
    1935549734,
    1938171179,
    1938172967,
    1938173140,
    1938817026,
    1939219752,
    1941178676,
    1941221172,
    1948778498,
    1963982850,
    1965115924,
    1965334268,
    1965634084,
    1966223078,
    1966386470,
    1967128578,
    1967760215,
    1967788867,
    1967795910,
    1967795958,
    1967957189,
    1968053806,
    1968836118,
    1968840263,
    1970798594,
    1970938456,
    1971457766,
    1971461414,
    1971465813,
    1971466997,
    1971467002,
    1971628838,
    1971703386,
    1971938532,
    1971981018,
    1973040373,
    1973420034,
    1974771450,
    1974775352,
    1976348214,
    1982106678,
    1982173479,
    1982935782,
    1983002201,
    1983533124,
    1983633431,
    1984294038,
    1986140359,
    1986351224,
    1986527234,
    1988486811,
    1988486813,
    1988502165,
    1988763672,
    1988972590,
    1989812374,
    1990037800,
    1990074116,
    1990969429,
    1990969577,
    1991350601,
    1991909525,
    1998585858,
    1998724870,
    1998883894,
    1999397992,
    1999745104,
    2000158722,
    2000439531,
    2000825752,
    2000965834,
    2001281328,
    2001309869,
    2001349704,
    2001349720,
    2001349736,
    2001392795,
    2001392796,
    2001392798,
    2001495140,
    2002780162,
    2002882873,
    2003062853,
    2003183333,
    2004557973,
    2004557976,
    2004635806,
    2004719812,
    2005160150,
    2005181733,
    2005231925,
    2005279787,
    2005324101,
    2005543977,
    2005543979,
    2005719336,
    2005766372,
    2005925890,
    2006028454,
    2006036556,
    2006329158,
    2006560839,
    2006592552,
    2006896969,
    2006974466,
    2007257240,
    2007601444,
    2007781534,
    2007803172,
    2008125638,
    2008133709,
    2008165414,
    2008340774,
    2008851557,
    2008994116,
    2009276567,
    2009706573,
    2021937364,
    2041712436,
    2051837468,
    2055514836,
    2055515017,
    2060065124,
    2066000646,
    2068523853,
    2068523856,
    2070023911,
    2072193862,
    2082727685,
    2083120164,
    2085266636,
    2087012585,
    2087049448,
    2091479332,
    2092255447,
    2092557349,
    };
}
