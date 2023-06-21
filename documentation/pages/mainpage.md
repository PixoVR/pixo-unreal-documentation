\mainpage About the SDK

This is an Unreal Engine plugin which will scan a project in search of Blueprints and Materials and publish them as parseable (fake) C++ code that can be fed into doxygen to produce documentation.

This has been tested with Unreal Version **4.27** and **5.x**.  The code should compile for both versions, and neither build is committed repository.  Instead it is compiled dynamically before/during documentation at the expense of (re)compile time, but with the advantage of not being locked to one version.

Graphs are published using graphviz dot syntax, which creates an SVG representation of Unreal graph structures.

In the future, more graph types will be included.  Behavior Trees, for example.

 - Github Repo: $(PROJECT_REPO)
 - Builder Repo: $(PROJECT_BUILDER_REPO)

# As an Editor plugin

Including this plugin in a project will create a `Pixo Documentation` menu item in Unreal's `Window` menu which will open an interface to produce documentation C++ files.

# As a Commandlet

This can also be called as a commandlet, which is the method used during cloud build.

`UE4Editor-Cmd.exe /path/to/project.uproject -run=PixoDocumentation -OutputMode=doxygen -OutputDir="/some/absolute/path" -Include="/path/to/thing1,/path/to/thing2"`

Where `OutputMode`, `OutputDir`, and `Include` are required, and `Include` is a comma-separated list of UFS paths.

# Build details

As Unreal is a large piece of software, some graphical (dot) representations will be inaccurate, and some links may be broken.  Please report these bugs so we can fix them!

When an Unreal plugin wants to publish its blueprints, materials, etc, this plugin does its work by being included in the shell project found at $(PROJECT_BUILDER_REPO).  This means that when a plugin is committed to `main`, the Dockerfile will pull in the UE image and compile and build what is needed to produce output.


