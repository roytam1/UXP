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

// uncomment to regenerate self
//import java.io.BufferedReader;
//import java.io.File;
//import java.io.FileInputStream;
//import java.io.IOException;
//import java.io.InputStreamReader;
//import java.util.Arrays;
//import java.util.Collections;
//import java.util.HashMap;
//import java.util.LinkedList;
//import java.util.List;
//import java.util.Map;
//import java.util.Map.Entry;
//import java.util.regex.Matcher;
//import java.util.regex.Pattern;

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
     * Indicates that the element is not a pre-interned element. Forbidden on
     * preinterned elements.
     */
    public static final int NOT_INTERNED = (1 << 30);

    /**
     * Indicates that the element is in the "special" category. This bit should
     * not be pre-set on MathML or SVG specials--only on HTML specials.
     */
    public static final int SPECIAL = (1 << 29);

    /**
     * The element is foster-parenting. This bit should be pre-set on elements
     * that are foster-parenting as HTML.
     */
    public static final int FOSTER_PARENTING = (1 << 28);

    /**
     * The element is scoping. This bit should be pre-set on elements that are
     * scoping as HTML.
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

    // CPPONLY: private @HtmlCreator Object htmlCreator;

    // CPPONLY: private @SvgCreator Object svgCreator;

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

    // CPPONLY: @Inline public @HtmlCreator Object getHtmlCreator() {
    // CPPONLY: return htmlCreator;
    // CPPONLY: }

    // CPPONLY: @Inline public @SvgCreator Object getSvgCreator() {
    // CPPONLY: return svgCreator;
    // CPPONLY: }

    @Inline public int getFlags() {
        return flags;
    }

    @Inline public int getGroup() {
        return flags & ElementName.GROUP_MASK;
    }

    @Inline public boolean isInterned() {
        return (flags & ElementName.NOT_INTERNED) == 0;
    }

    @Inline static int levelOrderBinarySearch(int[] data, int key) {
        int n = data.length;
        int i = 0;

        while (i < n) {
            int val = data[i];
            if (val < key) {
                i = 2 * i + 2;
            } else if (val > key) {
                i = 2 * i + 1;
            } else {
                return i;
            }
        }

        return -1;
    }

    @Inline static ElementName elementNameByBuffer(@NoLength char[] buf,
            int offset, int length, Interner interner) {
        @Unsigned int hash = ElementName.bufToHash(buf, length);
        int[] hashes;
        hashes = ElementName.ELEMENT_HASHES;
        int index = levelOrderBinarySearch(hashes, hash);
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
    @Inline private static @Unsigned int bufToHash(@NoLength char[] buf,
            int length) {
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
            // CPPONLY: @HtmlCreator Object htmlCreator, @SvgCreator Object
            // CPPONLY: svgCreator,
            int flags) {
        this.name = name;
        this.camelCaseName = camelCaseName;
        // CPPONLY: this.htmlCreator = htmlCreator;
        // CPPONLY: this.svgCreator = svgCreator;
        this.flags = flags;
    }

    public ElementName() {
        this.name = null;
        this.camelCaseName = null;
        // CPPONLY: this.htmlCreator = NS_NewHTMLUnknownElement;
        // CPPONLY: this.svgCreator = NS_NewSVGUnknownElement;
        this.flags = TreeBuilder.OTHER | NOT_INTERNED;
    }

    public void destructor() {
        // The translator adds refcount debug code here.
    }

    @Inline public void setNameForNonInterned(@Local String name
    // CPPONLY: , boolean custom
    ) {
        // No need to worry about refcounting the local name, because in the
        // C++ case the scoped atom table remembers its own atoms.
        this.name = name;
        this.camelCaseName = name;
        // CPPONLY: if (custom) {
        // CPPONLY: this.htmlCreator = NS_NewCustomElement;
        // CPPONLY: } else {
        // CPPONLY: this.htmlCreator = NS_NewHTMLUnknownElement;
        // CPPONLY: }
        // The assertion below relies on TreeBuilder.OTHER being zero!
        // TreeBuilder.OTHER isn't referenced here, because it would create
        // a circular C++ header dependency given that this method is inlined.
        assert this.flags == ElementName.NOT_INTERNED;
    }

    // CPPONLY: @Inline public boolean isCustom() {
    // CPPONLY: return this.htmlCreator == NS_NewCustomElement;
    // CPPONLY: }

    // CPPONLY: public static final ElementName ISINDEX = new ElementName(
    // CPPONLY:        "isindex", "isindex",
    // CPPONLY:         NS_NewHTMLUnknownElement, NS_NewSVGUnknownElement, 
    // CPPONLY:         TreeBuilder.ISINDEX | SPECIAL);
    // [NOCPP[
    public static final ElementName ISINDEX = new ElementName("isindex", "isindex", TreeBuilder.OTHER);
    // ]NOCPP]

    public static final ElementName ANNOTATION_XML = new ElementName(
            "annotation-xml", "annotation-xml",
            // CPPONLY: NS_NewHTMLUnknownElement, NS_NewSVGUnknownElement,
            TreeBuilder.ANNOTATION_XML | SCOPING_AS_MATHML);

    // START CODE ONLY USED FOR GENERATING CODE uncomment and run to regenerate

//    private static final Pattern HTML_TAG_DEF = Pattern.compile(
//            "^HTML_TAG\\(([^,]+),\\s*([^,]+),\\s*[^,]+\\).*$");
//
//    private static final Pattern HTML_HTMLELEMENT_TAG_DEF = Pattern.compile(
//            "^HTML_HTMLELEMENT_TAG\\(([^\\)]+)\\).*$");
//
//    private static final Pattern SVG_TAG_DEF = Pattern.compile(
//            "^SVG_(?:FROM_PARSER_)?TAG\\(([^,]+),\\s*([^\\)]+)\\).*$");
//
//    private static final Map<String, String> htmlMap = new HashMap<String, String>();
//
//    private static final Map<String, String> svgMap = new HashMap<String, String>();
//
//    private static void ingestHtmlTags(File htmlList) throws IOException {
//        // This doesn't need to be efficient, so let's make it easy to write.
//        BufferedReader htmlReader = new BufferedReader(
//                new InputStreamReader(new FileInputStream(htmlList), "utf-8"));
//        try {
//            String line;
//            while ((line = htmlReader.readLine()) != null) {
//                if (!line.startsWith("HTML_")) {
//                    continue;
//                }
//                if (line.startsWith("HTML_OTHER")) {
//                    continue;
//                }
//                Matcher m = HTML_TAG_DEF.matcher(line);
//                if (m.matches()) {
//                    String iface = m.group(2);
//                    if ("Unknown".equals(iface)) {
//                        continue;
//                    }
//                    htmlMap.put(m.group(1), "NS_NewHTML" + iface + "Element");
//                } else {
//                    m = HTML_HTMLELEMENT_TAG_DEF.matcher(line);
//                    if (!m.matches()) {
//                        throw new RuntimeException(
//                                "Malformed HTML element definition: " + line);
//                    }
//                    htmlMap.put(m.group(1), "NS_NewHTMLElement");
//                }
//            }
//        } finally {
//            htmlReader.close();
//        }
//    }
//
//    private static void ingestSvgTags(File svgList) throws IOException {
//        // This doesn't need to be efficient, so let's make it easy to write.
//        BufferedReader svgReader = new BufferedReader(
//                new InputStreamReader(new FileInputStream(svgList), "utf-8"));
//        try {
//            String line;
//            while ((line = svgReader.readLine()) != null) {
//                if (!line.startsWith("SVG_")) {
//                    continue;
//                }
//                Matcher m = SVG_TAG_DEF.matcher(line);
//                if (!m.matches()) {
//                    throw new RuntimeException(
//                            "Malformed SVG element definition: " + line);
//                }
//                String name = m.group(1);
//                if ("svgSwitch".equals(name)) {
//                    name = "switch";
//                }
//                svgMap.put(name, "NS_NewSVG" + m.group(2) + "Element");
//            }
//        } finally {
//            svgReader.close();
//        }
//    }
//
//    private static String htmlCreator(String name) {
//        String creator = htmlMap.remove(name);
//        if (creator != null) {
//            return creator;
//        }
//        return "NS_NewHTMLUnknownElement";
//    }
//
//    private static String svgCreator(String name) {
//        String creator = svgMap.remove(name);
//        if (creator != null) {
//            return creator;
//        }
//        return "NS_NewSVGUnknownElement";
//    }
//
//    /**
//     * @see java.lang.Object#toString()
//     */
//    @Override public String toString() {
//        return "(\"" + name + "\", \"" + camelCaseName + "\", \n// CPP"
//                + "ONLY: " + htmlCreator(name) + ",\n// CPP" + "ONLY: "
//                + svgCreator(camelCaseName) + ", \n" + decomposedFlags() + ")";
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
//            //  if (!"annotation-xml".equals(name)) {
//            //      throw new RuntimeException(
//            //              "Non-annotation-xml element name with hyphen: "
//            //                      + name);
//            //  }
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
//    private static void fillLevelOrderArray(List<ElementName> sorted, int depth,
//            int rootIdx, ElementName[] levelOrder) {
//        if (rootIdx >= levelOrder.length) {
//            return;
//        }
//
//        if (depth > 0) {
//            fillLevelOrderArray(sorted, depth - 1, rootIdx * 2 + 1, levelOrder);
//        }
//
//        if (!sorted.isEmpty()) {
//            levelOrder[rootIdx] = sorted.remove(0);
//        }
//
//        if (depth > 0) {
//            fillLevelOrderArray(sorted, depth - 1, rootIdx * 2 + 2, levelOrder);
//        }
//    }
//
//    /**
//     * Regenerate self
//     *
//     * The args should be the paths to m-c files
//     * parser/htmlparser/nsHTMLTagList.h and dom/svg/SVGTagList.h.
//     */
//    public static void main(String[] args) {
//        File htmlList = new File(args[0]);
//        File svgList = new File(args[1]);
//        try {
//            ingestHtmlTags(htmlList);
//        } catch (IOException e) {
//            throw new RuntimeException(e);
//        }
//        try {
//            ingestSvgTags(svgList);
//        } catch (IOException e) {
//            throw new RuntimeException(e);
//        }
//
//        Arrays.sort(ELEMENT_NAMES);
//        for (int i = 0; i < ELEMENT_NAMES.length; i++) {
//            int hash = ELEMENT_NAMES[i].hash();
//            if (hash < 0) {
//                System.err.println("Negative hash: " + ELEMENT_NAMES[i].name);
//                return;
//            }
//            for (int j = i + 1; j < ELEMENT_NAMES.length; j++) {
//                if (hash == ELEMENT_NAMES[j].hash()) {
//                    System.err.println(
//                            "Hash collision: " + ELEMENT_NAMES[i].name + ", "
//                                    + ELEMENT_NAMES[j].name);
//                    return;
//                }
//            }
//        }
//        for (int i = 0; i < ELEMENT_NAMES.length; i++) {
//            ElementName el = ELEMENT_NAMES[i];
//            if ("isindex".equals(el.name)) {
//                continue;
//            }
//            System.out.println(
//                    "public static final ElementName " + el.constName()
//                            + " = new ElementName" + el.toString() + ";");
//        }
//
//        LinkedList<ElementName> sortedNames = new LinkedList<ElementName>();
//        Collections.addAll(sortedNames, ELEMENT_NAMES);
//        ElementName[] levelOrder = new ElementName[ELEMENT_NAMES.length];
//        int bstDepth = (int) Math.ceil(
//                Math.log(ELEMENT_NAMES.length) / Math.log(2));
//        fillLevelOrderArray(sortedNames, bstDepth, 0, levelOrder);
//
//        System.out.println(
//                "private final static @NoLength ElementName[] ELEMENT_NAMES = {");
//        for (int i = 0; i < levelOrder.length; i++) {
//            ElementName el = levelOrder[i];
//            System.out.println(el.constName() + ",");
//        }
//        System.out.println("};");
//        System.out.println("private final static int[] ELEMENT_HASHES = {");
//        for (int i = 0; i < levelOrder.length; i++) {
//            ElementName el = levelOrder[i];
//            System.out.println(Integer.toString(el.hash()) + ",");
//        }
//        System.out.println("};");
//
//        for (Entry<String, String> entry : htmlMap.entrySet()) {
//            System.err.println("Missing HTML element: " + entry.getKey());
//        }
//        for (Entry<String, String> entry : svgMap.entrySet()) {
//            System.err.println("Missing SVG element: " + entry.getKey());
//        }
//    }


    // START GENERATED CODE
    public static final ElementName AND = new ElementName("and", "and", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ARG = new ElementName("arg", "arg", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ABS = new ElementName("abs", "abs", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName BIG = new ElementName("big", "big", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName BDO = new ElementName("bdo", "bdo", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName CSC = new ElementName("csc", "csc", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName COL = new ElementName("col", "col", 
    // CPPONLY: NS_NewHTMLTableColElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.COL | SPECIAL);
    public static final ElementName COS = new ElementName("cos", "cos", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName COT = new ElementName("cot", "cot", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DEL = new ElementName("del", "del", 
    // CPPONLY: NS_NewHTMLModElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DFN = new ElementName("dfn", "dfn", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DIR = new ElementName("dir", "dir", 
    // CPPONLY: NS_NewHTMLSharedElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName DIV = new ElementName("div", "div", 
    // CPPONLY: NS_NewHTMLDivElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
    public static final ElementName EXP = new ElementName("exp", "exp", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName GCD = new ElementName("gcd", "gcd", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName GEQ = new ElementName("geq", "geq", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName IMG = new ElementName("img", "img", 
    // CPPONLY: NS_NewHTMLImageElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.IMG | SPECIAL);
    public static final ElementName INS = new ElementName("ins", "ins", 
    // CPPONLY: NS_NewHTMLModElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName INT = new ElementName("int", "int", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName KBD = new ElementName("kbd", "kbd", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName LOG = new ElementName("log", "log", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName LCM = new ElementName("lcm", "lcm", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName LEQ = new ElementName("leq", "leq", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MTD = new ElementName("mtd", "mtd", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MIN = new ElementName("min", "min", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MAP = new ElementName("map", "map", 
    // CPPONLY: NS_NewHTMLMapElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MTR = new ElementName("mtr", "mtr", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MAX = new ElementName("max", "max", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName NEQ = new ElementName("neq", "neq", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName NOT = new ElementName("not", "not", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName NAV = new ElementName("nav", "nav", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName PRE = new ElementName("pre", "pre", 
    // CPPONLY: NS_NewHTMLPreElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.PRE_OR_LISTING | SPECIAL);
    public static final ElementName A = new ElementName("a", "a", 
    // CPPONLY: NS_NewHTMLAnchorElement,
    // CPPONLY: NS_NewSVGAElement, 
    TreeBuilder.A);
    public static final ElementName B = new ElementName("b", "b", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName RTC = new ElementName("rtc", "rtc", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.RB_OR_RTC | OPTIONAL_END_TAG);
    public static final ElementName REM = new ElementName("rem", "rem", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SUB = new ElementName("sub", "sub", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName SEC = new ElementName("sec", "sec", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SVG = new ElementName("svg", "svg", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGSVGElement, 
    TreeBuilder.SVG);
    public static final ElementName SUM = new ElementName("sum", "sum", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SIN = new ElementName("sin", "sin", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SEP = new ElementName("sep", "sep", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SUP = new ElementName("sup", "sup", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName SET = new ElementName("set", "set", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGSetElement, 
    TreeBuilder.OTHER);
    public static final ElementName TAN = new ElementName("tan", "tan", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName USE = new ElementName("use", "use", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUseElement, 
    TreeBuilder.OTHER);
    public static final ElementName VAR = new ElementName("var", "var", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName G = new ElementName("g", "g", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGGElement, 
    TreeBuilder.OTHER);
    public static final ElementName WBR = new ElementName("wbr", "wbr", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.AREA_OR_WBR | SPECIAL);
    public static final ElementName XMP = new ElementName("xmp", "xmp", 
    // CPPONLY: NS_NewHTMLPreElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.XMP | SPECIAL);
    public static final ElementName XOR = new ElementName("xor", "xor", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName I = new ElementName("i", "i", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName P = new ElementName("p", "p", 
    // CPPONLY: NS_NewHTMLParagraphElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.P | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName Q = new ElementName("q", "q", 
    // CPPONLY: NS_NewHTMLSharedElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName S = new ElementName("s", "s", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName U = new ElementName("u", "u", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName H1 = new ElementName("h1", "h1", 
    // CPPONLY: NS_NewHTMLHeadingElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H2 = new ElementName("h2", "h2", 
    // CPPONLY: NS_NewHTMLHeadingElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H3 = new ElementName("h3", "h3", 
    // CPPONLY: NS_NewHTMLHeadingElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H4 = new ElementName("h4", "h4", 
    // CPPONLY: NS_NewHTMLHeadingElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H5 = new ElementName("h5", "h5", 
    // CPPONLY: NS_NewHTMLHeadingElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName H6 = new ElementName("h6", "h6", 
    // CPPONLY: NS_NewHTMLHeadingElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.H1_OR_H2_OR_H3_OR_H4_OR_H5_OR_H6 | SPECIAL);
    public static final ElementName AREA = new ElementName("area", "area", 
    // CPPONLY: NS_NewHTMLAreaElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.AREA_OR_WBR | SPECIAL);
    public static final ElementName DATA = new ElementName("data", "data", 
    // CPPONLY: NS_NewHTMLDataElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName EULERGAMMA = new ElementName("eulergamma", "eulergamma", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FEFUNCA = new ElementName("fefunca", "feFuncA", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEFuncAElement, 
    TreeBuilder.OTHER);
    public static final ElementName LAMBDA = new ElementName("lambda", "lambda", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName METADATA = new ElementName("metadata", "metadata", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGMetadataElement, 
    TreeBuilder.OTHER);
    public static final ElementName META = new ElementName("meta", "meta", 
    // CPPONLY: NS_NewHTMLMetaElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.META | SPECIAL);
    public static final ElementName TEXTAREA = new ElementName("textarea", "textarea", 
    // CPPONLY: NS_NewHTMLTextAreaElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.TEXTAREA | SPECIAL);
    public static final ElementName FEFUNCB = new ElementName("fefuncb", "feFuncB", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEFuncBElement, 
    TreeBuilder.OTHER);
    public static final ElementName MSUB = new ElementName("msub", "msub", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName RB = new ElementName("rb", "rb", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.RB_OR_RTC | OPTIONAL_END_TAG);
    public static final ElementName ARCSEC = new ElementName("arcsec", "arcsec", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ARCCSC = new ElementName("arccsc", "arccsc", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DEFINITION_SRC = new ElementName("definition-src", "definition-src", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DESC = new ElementName("desc", "desc", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGDescElement, 
    TreeBuilder.FOREIGNOBJECT_OR_DESC | SCOPING_AS_SVG);
    public static final ElementName FONT_FACE_SRC = new ElementName("font-face-src", "font-face-src", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MFRAC = new ElementName("mfrac", "mfrac", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DD = new ElementName("dd", "dd", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.DD_OR_DT | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName BGSOUND = new ElementName("bgsound", "bgsound", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.LINK_OR_BASEFONT_OR_BGSOUND | SPECIAL);
    public static final ElementName CARD = new ElementName("card", "card", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DISCARD = new ElementName("discard", "discard", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName EMBED = new ElementName("embed", "embed", 
    // CPPONLY: NS_NewHTMLSharedObjectElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.EMBED | SPECIAL);
    public static final ElementName FEBLEND = new ElementName("feblend", "feBlend", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEBlendElement, 
    TreeBuilder.OTHER);
    public static final ElementName FEFLOOD = new ElementName("feflood", "feFlood", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEFloodElement, 
    TreeBuilder.OTHER);
    public static final ElementName GRAD = new ElementName("grad", "grad", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName HEAD = new ElementName("head", "head", 
    // CPPONLY: NS_NewHTMLSharedElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.HEAD | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName LEGEND = new ElementName("legend", "legend", 
    // CPPONLY: NS_NewHTMLLegendElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MFENCED = new ElementName("mfenced", "mfenced", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MPADDED = new ElementName("mpadded", "mpadded", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName NOEMBED = new ElementName("noembed", "noembed", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.NOEMBED | SPECIAL);
    public static final ElementName TD = new ElementName("td", "td", 
    // CPPONLY: NS_NewHTMLTableCellElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.TD_OR_TH | SPECIAL | SCOPING | OPTIONAL_END_TAG);
    public static final ElementName THEAD = new ElementName("thead", "thead", 
    // CPPONLY: NS_NewHTMLTableSectionElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.TBODY_OR_THEAD_OR_TFOOT | SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
    public static final ElementName ASIDE = new ElementName("aside", "aside", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName ARTICLE = new ElementName("article", "article", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName ANIMATE = new ElementName("animate", "animate", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGAnimateElement, 
    TreeBuilder.OTHER);
    public static final ElementName BASE = new ElementName("base", "base", 
    // CPPONLY: NS_NewHTMLSharedElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.BASE | SPECIAL);
    public static final ElementName BLOCKQUOTE = new ElementName("blockquote", "blockquote", 
    // CPPONLY: NS_NewHTMLSharedElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
    public static final ElementName CODE = new ElementName("code", "code", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName CIRCLE = new ElementName("circle", "circle", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGCircleElement, 
    TreeBuilder.OTHER);
    public static final ElementName COLOR_PROFILE = new ElementName("color-profile", "color-profile", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName COMPOSE = new ElementName("compose", "compose", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName CONJUGATE = new ElementName("conjugate", "conjugate", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName CITE = new ElementName("cite", "cite", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DIVERGENCE = new ElementName("divergence", "divergence", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DIVIDE = new ElementName("divide", "divide", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DEGREE = new ElementName("degree", "degree", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DECLARE = new ElementName("declare", "declare", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DATATEMPLATE = new ElementName("datatemplate", "datatemplate", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName EXPONENTIALE = new ElementName("exponentiale", "exponentiale", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ELLIPSE = new ElementName("ellipse", "ellipse", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGEllipseElement, 
    TreeBuilder.OTHER);
    public static final ElementName FONT_FACE = new ElementName("font-face", "font-face", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FETURBULENCE = new ElementName("feturbulence", "feTurbulence", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFETurbulenceElement, 
    TreeBuilder.OTHER);
    public static final ElementName FEMERGENODE = new ElementName("femergenode", "feMergeNode", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEMergeNodeElement, 
    TreeBuilder.OTHER);
    public static final ElementName FEIMAGE = new ElementName("feimage", "feImage", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEImageElement, 
    TreeBuilder.OTHER);
    public static final ElementName FEMERGE = new ElementName("femerge", "feMerge", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEMergeElement, 
    TreeBuilder.OTHER);
    public static final ElementName FETILE = new ElementName("fetile", "feTile", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFETileElement, 
    TreeBuilder.OTHER);
    public static final ElementName FONT_FACE_NAME = new ElementName("font-face-name", "font-face-name", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FRAME = new ElementName("frame", "frame", 
    // CPPONLY: NS_NewHTMLFrameElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.FRAME | SPECIAL);
    public static final ElementName FIGURE = new ElementName("figure", "figure", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName FALSE = new ElementName("false", "false", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FECOMPOSITE = new ElementName("fecomposite", "feComposite", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFECompositeElement, 
    TreeBuilder.OTHER);
    public static final ElementName IMAGE = new ElementName("image", "image", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGImageElement, 
    TreeBuilder.IMAGE);
    public static final ElementName IFRAME = new ElementName("iframe", "iframe", 
    // CPPONLY: NS_NewHTMLIFrameElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.IFRAME | SPECIAL);
    public static final ElementName INVERSE = new ElementName("inverse", "inverse", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName LINE = new ElementName("line", "line", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGLineElement, 
    TreeBuilder.OTHER);
    public static final ElementName LOGBASE = new ElementName("logbase", "logbase", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MSPACE = new ElementName("mspace", "mspace", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MODE = new ElementName("mode", "mode", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MARQUEE = new ElementName("marquee", "marquee", 
    // CPPONLY: NS_NewHTMLDivElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.MARQUEE_OR_APPLET | SPECIAL | SCOPING);
    public static final ElementName MTABLE = new ElementName("mtable", "mtable", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MSTYLE = new ElementName("mstyle", "mstyle", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MENCLOSE = new ElementName("menclose", "menclose", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName NONE = new ElementName("none", "none", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName OTHERWISE = new ElementName("otherwise", "otherwise", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName PIECE = new ElementName("piece", "piece", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName POLYLINE = new ElementName("polyline", "polyline", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGPolylineElement, 
    TreeBuilder.OTHER);
    public static final ElementName PICTURE = new ElementName("picture", "picture", 
    // CPPONLY: NS_NewHTMLPictureElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName PIECEWISE = new ElementName("piecewise", "piecewise", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName RULE = new ElementName("rule", "rule", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SOURCE = new ElementName("source", "source", 
    // CPPONLY: NS_NewHTMLSourceElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.PARAM_OR_SOURCE_OR_TRACK);
    public static final ElementName STRIKE = new ElementName("strike", "strike", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName STYLE = new ElementName("style", "style", 
    // CPPONLY: NS_NewHTMLStyleElement,
    // CPPONLY: NS_NewSVGStyleElement, 
    TreeBuilder.STYLE | SPECIAL);
    public static final ElementName TABLE = new ElementName("table", "table", 
    // CPPONLY: NS_NewHTMLTableElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.TABLE | SPECIAL | FOSTER_PARENTING | SCOPING);
    public static final ElementName TITLE = new ElementName("title", "title", 
    // CPPONLY: NS_NewHTMLTitleElement,
    // CPPONLY: NS_NewSVGTitleElement, 
    TreeBuilder.TITLE | SPECIAL | SCOPING_AS_SVG);
    public static final ElementName TIME = new ElementName("time", "time", 
    // CPPONLY: NS_NewHTMLTimeElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName TRANSPOSE = new ElementName("transpose", "transpose", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName TEMPLATE = new ElementName("template", "template", 
    // CPPONLY: NS_NewHTMLTemplateElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.TEMPLATE | SPECIAL | SCOPING);
    public static final ElementName TRUE = new ElementName("true", "true", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName VARIANCE = new ElementName("variance", "variance", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ALTGLYPHDEF = new ElementName("altglyphdef", "altGlyphDef", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DIFF = new ElementName("diff", "diff", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FACTOROF = new ElementName("factorof", "factorof", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName GLYPHREF = new ElementName("glyphref", "glyphRef", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName PARTIALDIFF = new ElementName("partialdiff", "partialdiff", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SETDIFF = new ElementName("setdiff", "setdiff", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName TREF = new ElementName("tref", "tref", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName CEILING = new ElementName("ceiling", "ceiling", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DIALOG = new ElementName("dialog", "dialog", 
    // CPPONLY: NS_NewHTMLDialogElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName FEFUNCG = new ElementName("fefuncg", "feFuncG", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEFuncGElement, 
    TreeBuilder.OTHER);
    public static final ElementName FEDIFFUSELIGHTING = new ElementName("fediffuselighting", "feDiffuseLighting", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEDiffuseLightingElement, 
    TreeBuilder.OTHER);
    public static final ElementName FESPECULARLIGHTING = new ElementName("fespecularlighting", "feSpecularLighting", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFESpecularLightingElement, 
    TreeBuilder.OTHER);
    public static final ElementName LISTING = new ElementName("listing", "listing", 
    // CPPONLY: NS_NewHTMLPreElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.PRE_OR_LISTING | SPECIAL);
    public static final ElementName STRONG = new ElementName("strong", "strong", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName ARCSECH = new ElementName("arcsech", "arcsech", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ARCCSCH = new ElementName("arccsch", "arccsch", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ARCTANH = new ElementName("arctanh", "arctanh", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ARCSINH = new ElementName("arcsinh", "arcsinh", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ALTGLYPH = new ElementName("altglyph", "altGlyph", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ARCCOSH = new ElementName("arccosh", "arccosh", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ARCCOTH = new ElementName("arccoth", "arccoth", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName CSCH = new ElementName("csch", "csch", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName COSH = new ElementName("cosh", "cosh", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName CLIPPATH = new ElementName("clippath", "clipPath", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGClipPathElement, 
    TreeBuilder.OTHER);
    public static final ElementName COTH = new ElementName("coth", "coth", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName GLYPH = new ElementName("glyph", "glyph", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MGLYPH = new ElementName("mglyph", "mglyph", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.MGLYPH_OR_MALIGNMARK);
    public static final ElementName MISSING_GLYPH = new ElementName("missing-glyph", "missing-glyph", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MATH = new ElementName("math", "math", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.MATH);
    public static final ElementName MPATH = new ElementName("mpath", "mpath", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGMPathElement, 
    TreeBuilder.OTHER);
    public static final ElementName PREFETCH = new ElementName("prefetch", "prefetch", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName PATH = new ElementName("path", "path", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGPathElement, 
    TreeBuilder.OTHER);
    public static final ElementName TH = new ElementName("th", "th", 
    // CPPONLY: NS_NewHTMLTableCellElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.TD_OR_TH | SPECIAL | SCOPING | OPTIONAL_END_TAG);
    public static final ElementName SECH = new ElementName("sech", "sech", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SWITCH = new ElementName("switch", "switch", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGSwitchElement, 
    TreeBuilder.OTHER);
    public static final ElementName SINH = new ElementName("sinh", "sinh", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName TANH = new ElementName("tanh", "tanh", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName TEXTPATH = new ElementName("textpath", "textPath", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGTextPathElement, 
    TreeBuilder.OTHER);
    public static final ElementName CI = new ElementName("ci", "ci", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FONT_FACE_URI = new ElementName("font-face-uri", "font-face-uri", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName LI = new ElementName("li", "li", 
    // CPPONLY: NS_NewHTMLLIElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.LI | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName IMAGINARYI = new ElementName("imaginaryi", "imaginaryi", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MI = new ElementName("mi", "mi", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName PI = new ElementName("pi", "pi", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName LINK = new ElementName("link", "link", 
    // CPPONLY: NS_NewHTMLLinkElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.LINK_OR_BASEFONT_OR_BGSOUND | SPECIAL);
    public static final ElementName MARK = new ElementName("mark", "mark", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MALIGNMARK = new ElementName("malignmark", "malignmark", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.MGLYPH_OR_MALIGNMARK);
    public static final ElementName MASK = new ElementName("mask", "mask", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGMaskElement, 
    TreeBuilder.OTHER);
    public static final ElementName TBREAK = new ElementName("tbreak", "tbreak", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName TRACK = new ElementName("track", "track", 
    // CPPONLY: NS_NewHTMLTrackElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.PARAM_OR_SOURCE_OR_TRACK | SPECIAL);
    public static final ElementName DL = new ElementName("dl", "dl", 
    // CPPONLY: NS_NewHTMLSharedListElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.UL_OR_OL_OR_DL | SPECIAL);
    public static final ElementName CSYMBOL = new ElementName("csymbol", "csymbol", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName CURL = new ElementName("curl", "curl", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FACTORIAL = new ElementName("factorial", "factorial", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FORALL = new ElementName("forall", "forall", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName HTML = new ElementName("html", "html", 
    // CPPONLY: NS_NewHTMLSharedElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.HTML | SPECIAL | SCOPING | OPTIONAL_END_TAG);
    public static final ElementName INTERVAL = new ElementName("interval", "interval", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName OL = new ElementName("ol", "ol", 
    // CPPONLY: NS_NewHTMLSharedListElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.UL_OR_OL_OR_DL | SPECIAL);
    public static final ElementName LABEL = new ElementName("label", "label", 
    // CPPONLY: NS_NewHTMLLabelElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName UL = new ElementName("ul", "ul", 
    // CPPONLY: NS_NewHTMLSharedListElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.UL_OR_OL_OR_DL | SPECIAL);
    public static final ElementName REAL = new ElementName("real", "real", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SMALL = new ElementName("small", "small", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName SYMBOL = new ElementName("symbol", "symbol", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGSymbolElement, 
    TreeBuilder.OTHER);
    public static final ElementName ALTGLYPHITEM = new ElementName("altglyphitem", "altGlyphItem", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ANIMATETRANSFORM = new ElementName("animatetransform", "animateTransform", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGAnimateTransformElement, 
    TreeBuilder.OTHER);
    public static final ElementName ACRONYM = new ElementName("acronym", "acronym", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName EM = new ElementName("em", "em", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName FORM = new ElementName("form", "form", 
    // CPPONLY: NS_NewHTMLFormElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.FORM | SPECIAL);
    public static final ElementName MENUITEM = new ElementName("menuitem", "menuitem", 
    // CPPONLY: NS_NewHTMLMenuItemElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.MENUITEM);
    public static final ElementName MPHANTOM = new ElementName("mphantom", "mphantom", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName PARAM = new ElementName("param", "param", 
    // CPPONLY: NS_NewHTMLSharedElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.PARAM_OR_SOURCE_OR_TRACK | SPECIAL);
    public static final ElementName CN = new ElementName("cn", "cn", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ARCTAN = new ElementName("arctan", "arctan", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ARCSIN = new ElementName("arcsin", "arcsin", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ANIMATION = new ElementName("animation", "animation", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ANNOTATION = new ElementName("annotation", "annotation", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ANIMATEMOTION = new ElementName("animatemotion", "animateMotion", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGAnimateMotionElement, 
    TreeBuilder.OTHER);
    public static final ElementName BUTTON = new ElementName("button", "button", 
    // CPPONLY: NS_NewHTMLButtonElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.BUTTON | SPECIAL);
    public static final ElementName FN = new ElementName("fn", "fn", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName CODOMAIN = new ElementName("codomain", "codomain", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName CAPTION = new ElementName("caption", "caption", 
    // CPPONLY: NS_NewHTMLTableCaptionElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.CAPTION | SPECIAL | SCOPING);
    public static final ElementName CONDITION = new ElementName("condition", "condition", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DOMAIN = new ElementName("domain", "domain", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DOMAINOFAPPLICATION = new ElementName("domainofapplication", "domainofapplication", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName IN = new ElementName("in", "in", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FIGCAPTION = new ElementName("figcaption", "figcaption", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName HKERN = new ElementName("hkern", "hkern", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName LN = new ElementName("ln", "ln", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MN = new ElementName("mn", "mn", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName KEYGEN = new ElementName("keygen", "keygen", 
    // CPPONLY: NS_NewHTMLSpanElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.KEYGEN);
    public static final ElementName LAPLACIAN = new ElementName("laplacian", "laplacian", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MEAN = new ElementName("mean", "mean", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MEDIAN = new ElementName("median", "median", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MAIN = new ElementName("main", "main", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName MACTION = new ElementName("maction", "maction", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName NOTIN = new ElementName("notin", "notin", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName OPTION = new ElementName("option", "option", 
    // CPPONLY: NS_NewHTMLOptionElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OPTION | OPTIONAL_END_TAG);
    public static final ElementName POLYGON = new ElementName("polygon", "polygon", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGPolygonElement, 
    TreeBuilder.OTHER);
    public static final ElementName PATTERN = new ElementName("pattern", "pattern", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGPatternElement, 
    TreeBuilder.OTHER);
    public static final ElementName RELN = new ElementName("reln", "reln", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SPAN = new ElementName("span", "span", 
    // CPPONLY: NS_NewHTMLSpanElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName SECTION = new ElementName("section", "section", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName TSPAN = new ElementName("tspan", "tspan", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGTSpanElement, 
    TreeBuilder.OTHER);
    public static final ElementName UNION = new ElementName("union", "union", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName VKERN = new ElementName("vkern", "vkern", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName AUDIO = new ElementName("audio", "audio", 
    // CPPONLY: NS_NewHTMLAudioElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MO = new ElementName("mo", "mo", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName TENDSTO = new ElementName("tendsto", "tendsto", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName VIDEO = new ElementName("video", "video", 
    // CPPONLY: NS_NewHTMLVideoElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName COLGROUP = new ElementName("colgroup", "colgroup", 
    // CPPONLY: NS_NewHTMLTableColElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.COLGROUP | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName FEDISPLACEMENTMAP = new ElementName("fedisplacementmap", "feDisplacementMap", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEDisplacementMapElement, 
    TreeBuilder.OTHER);
    public static final ElementName HGROUP = new ElementName("hgroup", "hgroup", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName MALIGNGROUP = new ElementName("maligngroup", "maligngroup", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MSUBSUP = new ElementName("msubsup", "msubsup", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MSUP = new ElementName("msup", "msup", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName RP = new ElementName("rp", "rp", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.RT_OR_RP | OPTIONAL_END_TAG);
    public static final ElementName OPTGROUP = new ElementName("optgroup", "optgroup", 
    // CPPONLY: NS_NewHTMLOptGroupElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OPTGROUP | OPTIONAL_END_TAG);
    public static final ElementName SAMP = new ElementName("samp", "samp", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName STOP = new ElementName("stop", "stop", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGStopElement, 
    TreeBuilder.OTHER);
    public static final ElementName EQ = new ElementName("eq", "eq", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName BR = new ElementName("br", "br", 
    // CPPONLY: NS_NewHTMLBRElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.BR | SPECIAL);
    public static final ElementName ABBR = new ElementName("abbr", "abbr", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ANIMATECOLOR = new ElementName("animatecolor", "animateColor", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName BVAR = new ElementName("bvar", "bvar", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName CENTER = new ElementName("center", "center", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
    public static final ElementName CURSOR = new ElementName("cursor", "cursor", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName HR = new ElementName("hr", "hr", 
    // CPPONLY: NS_NewHTMLHRElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.HR | SPECIAL);
    public static final ElementName FEFUNCR = new ElementName("fefuncr", "feFuncR", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEFuncRElement, 
    TreeBuilder.OTHER);
    public static final ElementName FECOMPONENTTRANSFER = new ElementName("fecomponenttransfer", "feComponentTransfer", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEComponentTransferElement, 
    TreeBuilder.OTHER);
    public static final ElementName FILTER = new ElementName("filter", "filter", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFilterElement, 
    TreeBuilder.OTHER);
    public static final ElementName FOOTER = new ElementName("footer", "footer", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName FLOOR = new ElementName("floor", "floor", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FEGAUSSIANBLUR = new ElementName("fegaussianblur", "feGaussianBlur", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEGaussianBlurElement, 
    TreeBuilder.OTHER);
    public static final ElementName HEADER = new ElementName("header", "header", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName HANDLER = new ElementName("handler", "handler", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName OR = new ElementName("or", "or", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName LISTENER = new ElementName("listener", "listener", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MUNDER = new ElementName("munder", "munder", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MARKER = new ElementName("marker", "marker", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGMarkerElement, 
    TreeBuilder.OTHER);
    public static final ElementName METER = new ElementName("meter", "meter", 
    // CPPONLY: NS_NewHTMLMeterElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MOVER = new ElementName("mover", "mover", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MUNDEROVER = new ElementName("munderover", "munderover", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MERROR = new ElementName("merror", "merror", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MLABELEDTR = new ElementName("mlabeledtr", "mlabeledtr", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName NOBR = new ElementName("nobr", "nobr", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.NOBR);
    public static final ElementName NOTANUMBER = new ElementName("notanumber", "notanumber", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName POWER = new ElementName("power", "power", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName TR = new ElementName("tr", "tr", 
    // CPPONLY: NS_NewHTMLTableRowElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.TR | SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
    public static final ElementName SOLIDCOLOR = new ElementName("solidcolor", "solidcolor", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SELECTOR = new ElementName("selector", "selector", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName VECTOR = new ElementName("vector", "vector", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ARCCOS = new ElementName("arccos", "arccos", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName ADDRESS = new ElementName("address", "address", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName CANVAS = new ElementName("canvas", "canvas", 
    // CPPONLY: NS_NewHTMLCanvasElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName COMPLEXES = new ElementName("complexes", "complexes", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DEFS = new ElementName("defs", "defs", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGDefsElement, 
    TreeBuilder.OTHER);
    public static final ElementName DETAILS = new ElementName("details", "details", 
    // CPPONLY: NS_NewHTMLDetailsElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName EXISTS = new ElementName("exists", "exists", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName IMPLIES = new ElementName("implies", "implies", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName INTEGERS = new ElementName("integers", "integers", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MS = new ElementName("ms", "ms", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName MPRESCRIPTS = new ElementName("mprescripts", "mprescripts", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MMULTISCRIPTS = new ElementName("mmultiscripts", "mmultiscripts", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MINUS = new ElementName("minus", "minus", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName NOFRAMES = new ElementName("noframes", "noframes", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.NOFRAMES | SPECIAL);
    public static final ElementName NATURALNUMBERS = new ElementName("naturalnumbers", "naturalnumbers", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName PRIMES = new ElementName("primes", "primes", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName PROGRESS = new ElementName("progress", "progress", 
    // CPPONLY: NS_NewHTMLProgressElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName PLUS = new ElementName("plus", "plus", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName REALS = new ElementName("reals", "reals", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName RATIONALS = new ElementName("rationals", "rationals", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SEMANTICS = new ElementName("semantics", "semantics", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName TIMES = new ElementName("times", "times", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DT = new ElementName("dt", "dt", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.DD_OR_DT | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName APPLET = new ElementName("applet", "applet", 
    // CPPONLY: NS_NewHTMLSharedObjectElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.MARQUEE_OR_APPLET | SPECIAL | SCOPING);
    public static final ElementName ARCCOT = new ElementName("arccot", "arccot", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName BASEFONT = new ElementName("basefont", "basefont", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.LINK_OR_BASEFONT_OR_BGSOUND | SPECIAL);
    public static final ElementName CARTESIANPRODUCT = new ElementName("cartesianproduct", "cartesianproduct", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName GT = new ElementName("gt", "gt", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DETERMINANT = new ElementName("determinant", "determinant", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName DATALIST = new ElementName("datalist", "datalist", 
    // CPPONLY: NS_NewHTMLDataListElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName EMPTYSET = new ElementName("emptyset", "emptyset", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName EQUIVALENT = new ElementName("equivalent", "equivalent", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FONT_FACE_FORMAT = new ElementName("font-face-format", "font-face-format", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FOREIGNOBJECT = new ElementName("foreignobject", "foreignObject", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGForeignObjectElement, 
    TreeBuilder.FOREIGNOBJECT_OR_DESC | SCOPING_AS_SVG);
    public static final ElementName FIELDSET = new ElementName("fieldset", "fieldset", 
    // CPPONLY: NS_NewHTMLFieldSetElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.FIELDSET | SPECIAL);
    public static final ElementName FRAMESET = new ElementName("frameset", "frameset", 
    // CPPONLY: NS_NewHTMLFrameSetElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.FRAMESET | SPECIAL);
    public static final ElementName FEOFFSET = new ElementName("feoffset", "feOffset", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEOffsetElement, 
    TreeBuilder.OTHER);
    public static final ElementName FESPOTLIGHT = new ElementName("fespotlight", "feSpotLight", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFESpotLightElement, 
    TreeBuilder.OTHER);
    public static final ElementName FEPOINTLIGHT = new ElementName("fepointlight", "fePointLight", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEPointLightElement, 
    TreeBuilder.OTHER);
    public static final ElementName FEDISTANTLIGHT = new ElementName("fedistantlight", "feDistantLight", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEDistantLightElement, 
    TreeBuilder.OTHER);
    public static final ElementName FONT = new ElementName("font", "font", 
    // CPPONLY: NS_NewHTMLFontElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.FONT);
    public static final ElementName LT = new ElementName("lt", "lt", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName INTERSECT = new ElementName("intersect", "intersect", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName IDENT = new ElementName("ident", "ident", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName INPUT = new ElementName("input", "input", 
    // CPPONLY: NS_NewHTMLInputElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.INPUT | SPECIAL);
    public static final ElementName LIMIT = new ElementName("limit", "limit", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName LOWLIMIT = new ElementName("lowlimit", "lowlimit", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName LINEARGRADIENT = new ElementName("lineargradient", "linearGradient", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGLinearGradientElement, 
    TreeBuilder.OTHER);
    public static final ElementName LIST = new ElementName("list", "list", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MOMENT = new ElementName("moment", "moment", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MROOT = new ElementName("mroot", "mroot", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MSQRT = new ElementName("msqrt", "msqrt", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MOMENTABOUT = new ElementName("momentabout", "momentabout", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MTEXT = new ElementName("mtext", "mtext", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.MI_MO_MN_MS_MTEXT | SCOPING_AS_MATHML);
    public static final ElementName NOTSUBSET = new ElementName("notsubset", "notsubset", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName NOTPRSUBSET = new ElementName("notprsubset", "notprsubset", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName NOSCRIPT = new ElementName("noscript", "noscript", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.NOSCRIPT | SPECIAL);
    public static final ElementName NEST = new ElementName("nest", "nest", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName RT = new ElementName("rt", "rt", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.RT_OR_RP | OPTIONAL_END_TAG);
    public static final ElementName OBJECT = new ElementName("object", "object", 
    // CPPONLY: NS_NewHTMLObjectElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OBJECT | SPECIAL | SCOPING);
    public static final ElementName OUTERPRODUCT = new ElementName("outerproduct", "outerproduct", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName OUTPUT = new ElementName("output", "output", 
    // CPPONLY: NS_NewHTMLOutputElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OUTPUT);
    public static final ElementName PRODUCT = new ElementName("product", "product", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName PRSUBSET = new ElementName("prsubset", "prsubset", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName PLAINTEXT = new ElementName("plaintext", "plaintext", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.PLAINTEXT | SPECIAL);
    public static final ElementName TT = new ElementName("tt", "tt", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.B_OR_BIG_OR_CODE_OR_EM_OR_I_OR_S_OR_SMALL_OR_STRIKE_OR_STRONG_OR_TT_OR_U);
    public static final ElementName QUOTIENT = new ElementName("quotient", "quotient", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName RECT = new ElementName("rect", "rect", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGRectElement, 
    TreeBuilder.OTHER);
    public static final ElementName RADIALGRADIENT = new ElementName("radialgradient", "radialGradient", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGRadialGradientElement, 
    TreeBuilder.OTHER);
    public static final ElementName ROOT = new ElementName("root", "root", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SELECT = new ElementName("select", "select", 
    // CPPONLY: NS_NewHTMLSelectElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.SELECT | SPECIAL);
    public static final ElementName SCALARPRODUCT = new ElementName("scalarproduct", "scalarproduct", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SUBSET = new ElementName("subset", "subset", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SLOT = new ElementName("slot", "slot", 
    // CPPONLY: NS_NewHTMLSlotElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName SCRIPT = new ElementName("script", "script", 
    // CPPONLY: NS_NewHTMLScriptElement,
    // CPPONLY: NS_NewSVGScriptElement, 
    TreeBuilder.SCRIPT | SPECIAL);
    public static final ElementName TFOOT = new ElementName("tfoot", "tfoot", 
    // CPPONLY: NS_NewHTMLTableSectionElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.TBODY_OR_THEAD_OR_TFOOT | SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
    public static final ElementName TEXT = new ElementName("text", "text", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGTextElement, 
    TreeBuilder.OTHER);
    public static final ElementName UPLIMIT = new ElementName("uplimit", "uplimit", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName VECTORPRODUCT = new ElementName("vectorproduct", "vectorproduct", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MENU = new ElementName("menu", "menu", 
    // CPPONLY: NS_NewHTMLMenuElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.DIV_OR_BLOCKQUOTE_OR_CENTER_OR_MENU | SPECIAL);
    public static final ElementName SDEV = new ElementName("sdev", "sdev", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FEDROPSHADOW = new ElementName("fedropshadow", "feDropShadow", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEDropShadowElement, 
    TreeBuilder.OTHER);
    public static final ElementName MROW = new ElementName("mrow", "mrow", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName MATRIXROW = new ElementName("matrixrow", "matrixrow", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName VIEW = new ElementName("view", "view", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGViewElement, 
    TreeBuilder.OTHER);
    public static final ElementName APPROX = new ElementName("approx", "approx", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName FECOLORMATRIX = new ElementName("fecolormatrix", "feColorMatrix", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEColorMatrixElement, 
    TreeBuilder.OTHER);
    public static final ElementName FECONVOLVEMATRIX = new ElementName("feconvolvematrix", "feConvolveMatrix", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEConvolveMatrixElement, 
    TreeBuilder.OTHER);
    public static final ElementName MATRIX = new ElementName("matrix", "matrix", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName APPLY = new ElementName("apply", "apply", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName BODY = new ElementName("body", "body", 
    // CPPONLY: NS_NewHTMLBodyElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.BODY | SPECIAL | OPTIONAL_END_TAG);
    public static final ElementName FEMORPHOLOGY = new ElementName("femorphology", "feMorphology", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGFEMorphologyElement, 
    TreeBuilder.OTHER);
    public static final ElementName IMAGINARY = new ElementName("imaginary", "imaginary", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName INFINITY = new ElementName("infinity", "infinity", 
    // CPPONLY: NS_NewHTMLUnknownElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.OTHER);
    public static final ElementName RUBY = new ElementName("ruby", "ruby", 
    // CPPONLY: NS_NewHTMLElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.RUBY_OR_SPAN_OR_SUB_OR_SUP_OR_VAR);
    public static final ElementName SUMMARY = new ElementName("summary", "summary", 
    // CPPONLY: NS_NewHTMLSummaryElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.ADDRESS_OR_ARTICLE_OR_ASIDE_OR_DETAILS_OR_DIALOG_OR_DIR_OR_FIGCAPTION_OR_FIGURE_OR_FOOTER_OR_HEADER_OR_HGROUP_OR_MAIN_OR_NAV_OR_SECTION_OR_SUMMARY | SPECIAL);
    public static final ElementName TBODY = new ElementName("tbody", "tbody", 
    // CPPONLY: NS_NewHTMLTableSectionElement,
    // CPPONLY: NS_NewSVGUnknownElement, 
    TreeBuilder.TBODY_OR_THEAD_OR_TFOOT | SPECIAL | FOSTER_PARENTING | OPTIONAL_END_TAG);
    private final static @NoLength ElementName[] ELEMENT_NAMES = {
    VKERN,
    LOGBASE,
    FIELDSET,
    DATA,
    LI,
    CANVAS,
    QUOTIENT,
    PRE,
    ARTICLE,
    DIALOG,
    ARCTAN,
    LISTENER,
    REALS,
    MROOT,
    MROW,
    GEQ,
    G,
    DD,
    ELLIPSE,
    STYLE,
    COTH,
    INTERVAL,
    MN,
    BR,
    NOTANUMBER,
    MPRESCRIPTS,
    CARTESIANPRODUCT,
    INTERSECT,
    RT,
    SCRIPT,
    APPLY,
    COS,
    MTD,
    SUM,
    U,
    MSUB,
    HEAD,
    CONJUGATE,
    FRAME,
    OTHERWISE,
    ALTGLYPHDEF,
    ARCTANH,
    TH,
    TBREAK,
    ANIMATETRANSFORM,
    CAPTION,
    OPTION,
    MALIGNGROUP,
    FECOMPONENTTRANSFER,
    MUNDEROVER,
    SELECTOR,
    EXISTS,
    NATURALNUMBERS,
    DT,
    EMPTYSET,
    FEPOINTLIGHT,
    LOWLIMIT,
    NOTSUBSET,
    PRODUCT,
    SELECT,
    VECTORPRODUCT,
    FECOLORMATRIX,
    INFINITY,
    BIG,
    DIR,
    KBD,
    MAX,
    REM,
    SET,
    I,
    H4,
    METADATA,
    DEFINITION_SRC,
    EMBED,
    NOEMBED,
    CODE,
    DEGREE,
    FEIMAGE,
    IMAGE,
    MTABLE,
    PIECEWISE,
    TRANSPOSE,
    PARTIALDIFF,
    LISTING,
    ARCCOTH,
    MATH,
    TANH,
    LINK,
    CURL,
    REAL,
    MENUITEM,
    ANIMATEMOTION,
    IN,
    MEDIAN,
    SPAN,
    VIDEO,
    OPTGROUP,
    CENTER,
    FEGAUSSIANBLUR,
    METER,
    MLABELEDTR,
    TR,
    ARCCOS,
    DEFS,
    INTEGERS,
    MINUS,
    PROGRESS,
    SEMANTICS,
    ARCCOT,
    DETERMINANT,
    FONT_FACE_FORMAT,
    FEOFFSET,
    FONT,
    INPUT,
    LIST,
    MOMENTABOUT,
    NOSCRIPT,
    OUTERPRODUCT,
    PLAINTEXT,
    RADIALGRADIENT,
    SUBSET,
    TEXT,
    SDEV,
    VIEW,
    ISINDEX,
    FEMORPHOLOGY,
    SUMMARY,
    ARG,
    CSC,
    DEL,
    EXP,
    INS,
    LCM,
    MAP,
    NOT,
    B,
    SEC,
    SEP,
    USE,
    XMP,
    Q,
    H2,
    H6,
    FEFUNCA,
    TEXTAREA,
    ARCSEC,
    FONT_FACE_SRC,
    CARD,
    FEFLOOD,
    MFENCED,
    THEAD,
    BASE,
    COLOR_PROFILE,
    DIVERGENCE,
    DATATEMPLATE,
    FETURBULENCE,
    FETILE,
    FALSE,
    INVERSE,
    MODE,
    MENCLOSE,
    POLYLINE,
    SOURCE,
    TITLE,
    TRUE,
    FACTOROF,
    TREF,
    FEDIFFUSELIGHTING,
    ARCSECH,
    ALTGLYPH,
    COSH,
    MGLYPH,
    PREFETCH,
    SWITCH,
    CI,
    MI,
    MALIGNMARK,
    DL,
    FORALL,
    LABEL,
    SYMBOL,
    EM,
    PARAM,
    ANIMATION,
    FN,
    DOMAIN,
    HKERN,
    LAPLACIAN,
    MACTION,
    PATTERN,
    TSPAN,
    MO,
    FEDISPLACEMENTMAP,
    MSUP,
    STOP,
    ANIMATECOLOR,
    HR,
    FOOTER,
    HANDLER,
    MARKER,
    MOVER,
    MERROR,
    NOBR,
    POWER,
    SOLIDCOLOR,
    VECTOR,
    ADDRESS,
    COMPLEXES,
    DETAILS,
    IMPLIES,
    MS,
    MMULTISCRIPTS,
    NOFRAMES,
    PRIMES,
    PLUS,
    RATIONALS,
    TIMES,
    APPLET,
    BASEFONT,
    GT,
    DATALIST,
    EQUIVALENT,
    FOREIGNOBJECT,
    FRAMESET,
    FESPOTLIGHT,
    FEDISTANTLIGHT,
    LT,
    IDENT,
    LIMIT,
    LINEARGRADIENT,
    MOMENT,
    MSQRT,
    MTEXT,
    NOTPRSUBSET,
    NEST,
    OBJECT,
    OUTPUT,
    PRSUBSET,
    TT,
    RECT,
    ROOT,
    SCALARPRODUCT,
    SLOT,
    TFOOT,
    UPLIMIT,
    MENU,
    FEDROPSHADOW,
    MATRIXROW,
    APPROX,
    FECONVOLVEMATRIX,
    MATRIX,
    BODY,
    IMAGINARY,
    RUBY,
    TBODY,
    AND,
    ABS,
    BDO,
    COL,
    COT,
    DFN,
    DIV,
    GCD,
    IMG,
    INT,
    LOG,
    LEQ,
    MIN,
    MTR,
    NEQ,
    NAV,
    A,
    RTC,
    SUB,
    SVG,
    SIN,
    SUP,
    TAN,
    VAR,
    WBR,
    XOR,
    P,
    S,
    H1,
    H3,
    H5,
    AREA,
    EULERGAMMA,
    LAMBDA,
    META,
    FEFUNCB,
    RB,
    ARCCSC,
    DESC,
    MFRAC,
    BGSOUND,
    DISCARD,
    FEBLEND,
    GRAD,
    LEGEND,
    MPADDED,
    TD,
    ASIDE,
    ANIMATE,
    BLOCKQUOTE,
    CIRCLE,
    COMPOSE,
    CITE,
    DIVIDE,
    DECLARE,
    EXPONENTIALE,
    FONT_FACE,
    FEMERGENODE,
    FEMERGE,
    FONT_FACE_NAME,
    FIGURE,
    FECOMPOSITE,
    IFRAME,
    LINE,
    MSPACE,
    MARQUEE,
    MSTYLE,
    NONE,
    PIECE,
    PICTURE,
    RULE,
    STRIKE,
    TABLE,
    TIME,
    TEMPLATE,
    VARIANCE,
    DIFF,
    GLYPHREF,
    SETDIFF,
    CEILING,
    FEFUNCG,
    FESPECULARLIGHTING,
    STRONG,
    ARCCSCH,
    ARCSINH,
    ARCCOSH,
    CSCH,
    CLIPPATH,
    GLYPH,
    MISSING_GLYPH,
    MPATH,
    PATH,
    SECH,
    SINH,
    TEXTPATH,
    FONT_FACE_URI,
    IMAGINARYI,
    PI,
    MARK,
    MASK,
    TRACK,
    CSYMBOL,
    FACTORIAL,
    HTML,
    OL,
    UL,
    SMALL,
    ALTGLYPHITEM,
    ACRONYM,
    FORM,
    MPHANTOM,
    CN,
    ARCSIN,
    ANNOTATION,
    BUTTON,
    CODOMAIN,
    CONDITION,
    DOMAINOFAPPLICATION,
    FIGCAPTION,
    LN,
    KEYGEN,
    MEAN,
    MAIN,
    NOTIN,
    POLYGON,
    RELN,
    SECTION,
    UNION,
    AUDIO,
    TENDSTO,
    COLGROUP,
    HGROUP,
    MSUBSUP,
    RP,
    SAMP,
    EQ,
    ABBR,
    BVAR,
    CURSOR,
    FEFUNCR,
    FILTER,
    FLOOR,
    HEADER,
    OR,
    MUNDER,
    };
    private final static int[] ELEMENT_HASHES = {
    1909280949,
    1753057319,
    2001349704,
    1681770564,
    1818230786,
    1982935782,
    2007257240,
    58773795,
    1747176599,
    1782357526,
    1897999926,
    1970938456,
    1990969429,
    2005181733,
    2055514836,
    54061139,
    62390273,
    1730150402,
    1749395095,
    1756625221,
    1798693940,
    1868641064,
    1902641154,
    1963982850,
    1971981018,
    1988486811,
    1999745104,
    2002882873,
    2005925890,
    2008340774,
    2082727685,
    51965171,
    57200451,
    60350803,
    69730305,
    1703292116,
    1733890180,
    1748355193,
    1749813541,
    1754634617,
    1763839627,
    1797540167,
    1805647874,
    1857622310,
    1881498736,
    1899272519,
    1905563974,
    1938171179,
    1967788867,
    1971467002,
    1974775352,
    1984294038,
    1988972590,
    1998585858,
    2000825752,
    2001392796,
    2004557976,
    2005543977,
    2006560839,
    2008125638,
    2009706573,
    2068523853,
    2087049448,
    51434643,
    52488851,
    56151587,
    57210387,
    59826259,
    60354131,
    63438849,
    926941186,
    1686489160,
    1715300574,
    1732381397,
    1737099991,
    1748100148,
    1748642422,
    1749715159,
    1751288021,
    1753467414,
    1755158905,
    1757259017,
    1771722827,
    1786534215,
    1797645367,
    1803929812,
    1807501636,
    1853642948,
    1865773108,
    1873350948,
    1887579800,
    1898223949,
    1900544002,
    1904285766,
    1907435316,
    1925844629,
    1939219752,
    1966223078,
    1968053806,
    1971465813,
    1971703386,
    1973420034,
    1982106678,
    1983533124,
    1986351224,
    1988502165,
    1990037800,
    1991350601,
    1998883894,
    2000439531,
    2001281328,
    2001349736,
    2001495140,
    2003183333,
    2004719812,
    2005279787,
    2005719336,
    2006036556,
    2006896969,
    2007781534,
    2008165414,
    2008994116,
    2041712436,
    2060065124,
    2070023911,
    2085266636,
    2092255447,
    50910499,
    51957043,
    52485715,
    53012355,
    55110883,
    56680499,
    57206291,
    57732851,
    59768833,
    60345427,
    60352083,
    61395251,
    62973651,
    67633153,
    893386754,
    960495618,
    1682547543,
    1689922072,
    1713515574,
    1716349149,
    1731545140,
    1733076167,
    1736576231,
    1740181637,
    1747814436,
    1748228205,
    1748607578,
    1748879564,
    1749656156,
    1749801286,
    1749917205,
    1751493207,
    1753343188,
    1753588936,
    1755076808,
    1756474198,
    1757146773,
    1757293380,
    1766632184,
    1773808452,
    1783388497,
    1797361975,
    1797585096,
    1798677556,
    1803876550,
    1805233752,
    1806806678,
    1813512194,
    1818755074,
    1854228698,
    1864368130,
    1867237670,
    1870268949,
    1874102998,
    1881669634,
    1889085973,
    1898223945,
    1898971138,
    1899694294,
    1901940917,
    1903761465,
    1904515399,
    1906135367,
    1907959605,
    1919418370,
    1934172497,
    1938173140,
    1941221172,
    1965334268,
    1967128578,
    1967795958,
    1968840263,
    1971461414,
    1971466997,
    1971628838,
    1971938532,
    1973040373,
    1974771450,
    1976348214,
    1982173479,
    1983002201,
    1983633431,
    1986140359,
    1986527234,
    1988486813,
    1988763672,
    1989812374,
    1990074116,
    1990969577,
    1991909525,
    1998724870,
    1999397992,
    2000158722,
    2000525512,
    2000965834,
    2001309869,
    2001349720,
    2001392795,
    2001392798,
    2002780162,
    2003062853,
    2004557973,
    2004635806,
    2005160150,
    2005231925,
    2005324101,
    2005543979,
    2005766372,
    2006028454,
    2006329158,
    2006592552,
    2006974466,
    2007601444,
    2007803172,
    2008133709,
    2008325940,
    2008851557,
    2009276567,
    2021937364,
    2051837468,
    2055515017,
    2066000646,
    2068523856,
    2072193862,
    2083120164,
    2087012585,
    2091479332,
    2092557349,
    50908899,
    50916387,
    51438659,
    51961587,
    51965683,
    52486755,
    52490899,
    54054451,
    55104723,
    55111395,
    56677619,
    56682579,
    57205395,
    57207619,
    57731155,
    57733651,
    59244545,
    59821379,
    60345171,
    60347747,
    60351123,
    60352339,
    60875283,
    61925907,
    62450211,
    62974707,
    67108865,
    68681729,
    876609538,
    910163970,
    943718402,
    1679960596,
    1682186266,
    1685703382,
    1686491348,
    1699324759,
    1703936002,
    1713736758,
    1715310660,
    1719741029,
    1730965751,
    1732069431,
    1733054663,
    1733372532,
    1736200310,
    1736576583,
    1738539010,
    1747048757,
    1747306711,
    1747838298,
    1748225318,
    1748346119,
    1748359220,
    1748621670,
    1748846791,
    1749272732,
    1749649513,
    1749673195,
    1749723735,
    1749813486,
    1749905526,
    1749932347,
    1751386406,
    1752979652,
    1753319686,
    1753362711,
    1753479494,
    1754031332,
    1754894485,
    1755148615,
    1756098852,
    1756600614,
    1757137429,
    1757157700,
    1757268168,
    1758044696,
    1765431364,
    1766992520,
    1773295687,
    1781815495,
    1783210839,
    1783388498,
    1790207270,
    1797368887,
    1797544247,
    1797628983,
    1798417460,
    1798686984,
    1800730821,
    1803876557,
    1803929861,
    1805502724,
    1806799156,
    1806981428,
    1807599880,
    1817013469,
    1818700314,
    1820327938,
    1854228692,
    1854245076,
    1857653029,
    1865714391,
    1867061545,
    1868312196,
    1870135298,
    1873281026,
    1874053333,
    1881288348,
    1881613047,
    1884120164,
    1887743720,
    1897398274,
    1898130486,
    1898223946,
    1898753862,
    1899170008,
    1899272521,
    1899796819,
    1900845386,
    1902116866,
    1903302038,
    1904283860,
    1904412884,
    1904946933,
    1906087319,
    1907085604,
    1907661127,
    1908709605,
    1914900309,
    1925049415,
    1932928296,
    1935549734,
    1938172967,
    1938817026,
    1941178676,
    1948778498,
    1965115924,
    1965634084,
    1966386470,
    1967760215,
    1967795910,
    1967957189,
    1968836118,
    1970798594,
    1971457766,
    };
}
