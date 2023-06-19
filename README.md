# pixo-unreal-documentation

This is an Unreal Engine plugin which will scan a project in search of Blueprints and Materials and publish them as parseable (fake) C++ code that can be fed into doxygen to produce documentation.

This has been tested with Unreal Version **4.27** and **5.x**.  The code should compile for both versions, however only the **4.27** build is committed to this repository.  For UE5, it can compile on the fly during documentation, at the expense of a moment of (re)compile time.

Graphs are published using graphviz dot syntax, which creates an SVG representation of Unreal graph structures.



