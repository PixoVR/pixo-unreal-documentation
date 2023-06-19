\mainpage About the SDK

This is an Unreal Engine plugin which will scan a project in search of Blueprints and Materials and publish them as parseable (fake) C++ code that can be fed into doxygen to produce documentation.

This has been tested with Unreal Version **4.27** and **5.x**.

Graphs are published using graphviz dot syntax, which creates an SVG representaion of Unreal graph structures.

In the future, more graph types will be included.  Behavior Trees, for example.

Useful links:

 - Github Repo: $(PROJECT_REPO)
 - Demo: \todo add a demo link

Including this plugin in a project will create a `Pixo Documentation` menu item (in Window), and from there documentation (C++ files) can be produced from within the editor.

It can also be called as a commandlet:

`UE4Editor-Cmd.exe $PROJECT.uproject -run=PixoDocumentation -OutputMode=doxygen -OutputDir="$OUTPUT_DIR" -Include="/path/to/thing1,/path/to/thing2"`

Where `OutputMode`, `OutputDir`, and `Include` are required, and `Include` is a comma-separated list of UFS paths.

As Unreal is a large piece of software, some graphical (dot) representations will be inaccurate, and some links may be broken.  Please report these bugs so we can find them all!

