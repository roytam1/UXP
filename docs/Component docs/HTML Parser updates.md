# Updating HTML5 parser code

Our html5 parser is based on the java html5 parser from [Validator.nu](http://about.validator.nu/htmlparser/) by Henri Sivonen. It has been adopted by Mozilla and further updated, and has been imported as a whole into the UXP tree to have an independent and maintainable source of it that doesn't rely on external sources.

## Stages
Updating the parser code consists of 3 stages:
- Make updates to the html parser source in java
- Let the java parser regenerate part of its own code after the change
- Translate the java source to C++

This process was best explained in the [following Bugzilla comment](https://bugzilla.mozilla.org/show_bug.cgi?id=1378079#c6), which explain how to add a new attribute name ("is") to html5, inserted in this document for convenience:

>> Is
>> there any documentation on how to add a new nsHtml5AttributeName?
>
> I don't recall. I should get around to writing it.
>
>> Looks like
>> I need to clone hg.mozilla.org/projects/htmlparser/ and generate a hash with
>> it?
>
> Yes. Here's how:
>
> `cd parser/html/java/`
> `make sync`
>
> Now you have a clone of [https://hg.mozilla.org/projects/htmlparser/](https://hg.mozilla.org/projects/htmlparser/) in > parser/html/java/htmlparser/
>
> `cd htmlparser/src/`
> `$EDITOR nu/validator/htmlparser/impl/AttributeName.java`
>
> Search for the word "uncomment" and uncomment stuff according to the two comments that talk about uncommenting
> Duplicate the declaration a normal attribute (nothings special in SVG mode, etc.). Let's use "alt", since it's the first one.
> In the duplicate, replace ALT with IS and "alt" with "is".
> Search for "ALT,", duplicate that line and change the duplicate to say "IS,"
> Save.
>
> `javac nu/validator/htmlparser/impl/AttributeName.java`
> `java nu.validator.htmlparser.impl.AttributeName`
>
> Copy and paste the output into nu/validator/htmlparser/impl/AttributeName.java replacing the text below the comment "START GENERATED CODE" and above the very last "}".
> Recomment the bits that you uncommented earlier.
> Save.
>
> `cd ../..` - Back to parser/html/java/
> `make translate`

## Organizing commits

**The html5 parser code is fragile due to its generation and translation before being used as C++ in our tree. Do not touch or commit anything without a code peer nearby with knowledge of the parser and the commit process (at this moment that means Gaming4JC (@g4jc)), and communicate the changes thoroughly.**

To organize this properly in our repo, commits should be split up when making these kinds of changes:
1. Commit your code edits to the html parser
2. Regenerate java into a translation-ready source
3. Commit
4. Translate and regenerate C++ code
5. Check a build to make sure the changes have the intended result
6. Commit

This is needed because the source edit will sometimes be in parts that are self-generated and may otherwise be lost in generation noise, and because we want to keep a strict separation between commits resulting from developer work and those resulting from running scripts/automated processes.



