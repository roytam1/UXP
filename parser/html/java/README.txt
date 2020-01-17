If this is your first time building the HTML5 parser, you need to execute the
following commands (from this directory) to accomplish the translation:

  make translate        # perform the Java-to-C++ translation from the remote 
                        # sources
  make named_characters # Generate tables for named character tokenization

If you make changes to the translator or the javaparser, you can rebuild by
retyping 'make' in this directory.  If you make changes to the HTML5 Java
implementation, you can retranslate the Java sources from the htmlparser 
repository by retyping 'make translate' in this directory.

The makefile supports the following targets:

javaparser:
  Builds the javaparser library retrieved earlier by sync_javaparser. 
translator:
  Runs the javaparser target and then builds the Java to C++ translator from 
  sources.
libs:
  The default target. Alias for translator
translate:
  Runs the translator target and then translates the HTML parser sources and
  copys the parser impl java sources to ../javasrc.
translate-javasrc: 
  Runs the translator target and then translates the HTML parser sources 
  stored in ../javasrc. (Depercated)
named-characters:
  Generates data tables for named character tokenization.
clean_-avaparser:
  Removes the build products of the javaparser target.
clean-htmlparser:
  Removes the build products of the translator target.
clean-javasrc:
  Removes the javasrc snapshot code in ../javasrc
clean:
  Runs clean-javaparser, clean-htmlparser, and clean-javasrc.

Ben Newman (23 September 2009)
Henri Sivonen (11 August 2016)
Matt A. Tobin (16 January 2020)
