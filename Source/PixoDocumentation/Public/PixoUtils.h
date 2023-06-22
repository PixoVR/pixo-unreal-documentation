// (c) 2023 PixoVR

#pragma once

#include "CoreMinimal.h"

/**
 * @brief operator <<
 * @param out
 * @param c
 * @return reference to the incoming wostream
 *
 * This is a gross workaround for Linux compilation.  TCHAR*
 * is not properly handled in Unreal's workaround for C++ stdlib
 * libraries.  Without this override, TCHAR* is seen as a pointer
 * and results in hex memory addresses on the stream.
 *
 * Note that this will convert a `TCHAR*` to a `wchar_t*` via
 * cast, so it's not doing any character set conversion.
 */

std::wostream & operator << (std::wostream &out, const TCHAR *c)
{
	//cout << "override!\n";
	out << TCHAR_TO_ANSI(c);
    return out;
}

namespace PixoUtils
{
	typedef TMap<FString, FString> vmap;	//name value pairs

	//... could use this instead of vmap
	//FFormatNamedArguments Args;
	//Args.Add(TEXT("NodeName"), FText::FromString(GraphNode->GetName()));
	//Args.Add(TEXT("TooltipText"), TooltipText);
	//TooltipText = FText::Format(NSLOCTEXT("GraphEditor", "GraphNodeTooltip", "{NodeName}\n\n{TooltipText}"), Args);

	//modes for reporting output
	enum OutputMode
	{
		none	= 0x0 << 0,
		verbose	= 0x1 << 0,
		debug	= 0x1 << 1,
		doxygen	= 0x1 << 2,
		MAX	= 0x1 << 3
	};

	TMap<FString, OutputMode> OutputMode_e = {
		{ "none",		OutputMode::none	},
		{ "verbose",	OutputMode::verbose	},
		{ "debug",		OutputMode::debug	},
		{ "doxygen",	OutputMode::doxygen	},
		{ "MAX",		OutputMode::MAX		}
	};

	//different nodes have to output differently
	enum class NodeType : int
	{
		none = 0,
		node,
		comment,
		bubble,
		route,
		variable,
		variableset,
		compact,
		composite,
		function,
		macro,
		tunnel,
		event,
		path,
		spawn,
		materialnode,
		materialcomposite,
		materialtunnel,
		MAX
	};

	TMap<FString, NodeType> NodeType_e = {
		{ "none",						NodeType::none			},
		{ "node",						NodeType::node			},
		{ "comment",					NodeType::comment		},
		{	"EdGraphNode_Comment",		NodeType::comment		},
		{	"MaterialGraphNode_Comment",NodeType::comment		},
		{ "bubble",						NodeType::bubble		},
		{ "path",						NodeType::path			},	//TODO: no instances of this yet
		{ "route",						NodeType::route			},
		{	"K2Node_Knot",				NodeType::route			},
		{	"MaterialGraphNode_Knot",	NodeType::route			},
		{ "variable",					NodeType::variable		},
		{	"K2Node_VariableGet",		NodeType::variable		},
		{ "variableset",				NodeType::variableset	},
		{	"K2Node_VariableSet",		NodeType::variableset	},
		{ "compact",					NodeType::compact		},
		{	"K2Node_PromotableOperator",NodeType::compact		},
		{ "composite",					NodeType::composite		},
		{	"K2Node_Composite",			NodeType::composite		},
		{ "function",					NodeType::function		},
		{	"K2Node_CallFunction",		NodeType::function		},
		{	"K2Node_CallParentFunction",NodeType::function		},	//NO: FunctionEntry
		{ "macro",						NodeType::macro			},	//dimmer header, but normal node
		{	"K2Node_MacroInstance",		NodeType::macro			},
		{ "tunnel",						NodeType::tunnel		},
		{	"K2Node_Tunnel",			NodeType::tunnel		},
		{ "event",						NodeType::event			},
		{	"K2Node_Event",				NodeType::event			},
		{	"K2Node_CustomEvent",		NodeType::event			},
		{ "spawn",						NodeType::spawn			},
		{	"K2Node_SpawnActor",		NodeType::spawn			},
		{	"K2Node_SpawnActorFromClass",NodeType::spawn		},
		{	"K2Node_GenericCreateObject",NodeType::spawn		},
		{	"K2Node_ConstructObjectFromClass",NodeType::spawn	},
		{	"K2Node_DynamicCast",		NodeType::spawn			},
		{	"K2Node_ClassDynamicCast",	NodeType::spawn			},
		{ "materialnode",				NodeType::materialnode	},
		{	"MaterialGraphNode_Root",	NodeType::materialnode	},
		{	"MaterialGraphNode",		NodeType::materialnode	},
		{ "materialcomposite",			NodeType::materialcomposite},
		{	"MaterialGraphNode_Composite",NodeType::materialcomposite},
		{ "materialtunnel",				NodeType::materialtunnel},
		{	"MaterialGraphNode_PinBase",NodeType::materialtunnel},
		{ "MAX",						NodeType::MAX			}
	};

	TMap<FString, FString> NodeIcons = {
		{ "default",	"&#10767;"	},		// function symbol	&#10765; &#10767;
		{ "event",		"&#10070;"	},		// diamond		&#10070; &#9672; &#11030; &#11031;
		{ "container",	"&#8651;"	},		// arrows?
		{ "switch",		"&#8997;"	},		// switch
		{ "path",		"&#8916;"	},		// fork in the road
		{ "io",			"&#10140;"	},		// input/output circle arrow		&#8658; &#128468;
		{ "spawn",		"&#10038;"	}		// star			&#9733; &#10038; //https://www.amp-what.com/unicode/search/star
	};

	typedef TPair<FString, FString> TPSS;
	TMap<FString, TPair<FString,FString> > PinIcons = {
		//{ "example",	{ "conicon", "disicon" } },		// connected/disconnected, or closed/open
		{ "data",		TPSS("&#9678;", "&#9673;") },					// open/closed circle
		{ "exec",		TPSS("&#9655;&#65038;", "&#9654;&#65038;") },	// open/closed right arrow. Uses &#65038; after to request the non-emoji representation.
		{ "delegate",	TPSS("&#9634;", "&#9635;") },					// open/closed box
		{ "addpin",		TPSS("&#9678;", "&#9673;") },					// open/closed circle		// &#8853; &#10753;	//https://www.isthisthingon.org/unicode/index.phtml?page=02&subpage=A
		{ "container",	TPSS("&#9920;", "&#9923;") },					// open/closed barrel thing
		//{ "array",		{"&#10214;&#9678;&#10215;","&#10214;&#9673;&#10215;"}},		// open/closed square braces
		{ "array",		TPSS("[&#9678;]","[&#9673;]") },				// open/closed square braces	&#128992; grid circle thing
		{ "map",		TPSS("&#10218;&#9638;&#10219;","&#10218;&#9641;&#10219;") },	// open/closed angle bracket
		{ "set",		TPSS("&#10100;&#9678;&#10101;","&#10100;&#9673;&#10101;") },	// open/closed curly braces
		{ "skull",		TPSS("&#9760;", "&#9760;") },					// skull		// to remember
		{ "coffee",		TPSS("&#9982;", "&#9982;") },					// coffee		// to remember
		{ "dotthing",	TPSS("&#10690;","&#10690;") }					// circle dot thing			// to remember
	};

	FString htmlentities(FString in);
	void RGBtoHSL(float R, float G, float B, float* h, float* s, float* l);
	void HSLtoRGB(float H, float S, float L, float* r, float* g, float* b);

	FString createColorString(FLinearColor color, float alpha = 1.0f, float exponent = 1.0f);
	FString createVariableName(FString name);

	bool createThumbnailFile(UObject* object, FString pngPath);

	FString getClassName(UClass* _class);

	//file stuff
	FString getTrimmedConfigFilePath(FString path);
	FString prepTemplateString(FString prefix, vmap style, FString string);
	FString prepNodePortRows(FString prefix, UEdGraphNode* node, TMap<FString, FString> visiblePins = TMap<FString, FString>());

	//graph stuff
	void addAllGraphs(TArray<UEdGraph*> &container, TArray<UEdGraph*> &graphs);

	//node stuff
	NodeType getNodeType(UEdGraphNode* node, NodeType defaultType=NodeType::none);
	FString getNodeTypeGroup(NodeType type);
	FString getNodeTooltip(UEdGraphNode* node);
	FString getNodeIcon(UEdGraphNode* node);
	FString getNodeTemplate(NodeType type, bool hasDelegate=false);

	//pin stuff
	FString getPinLabel(UEdGraphPin* pin);
	FString getPinTooltip(UEdGraphPin* p, TMap<FString,FString>visiblePins = TMap<FString, FString>());
	FString getPinType(UEdGraphPin* pin, bool useSchema=false);
	FString getPinPort(UEdGraphPin* p);
	FString getPinDefaultValue(UEdGraphPin* pin);
	FString getPinColor(UEdGraphPin* pin);
	FString getPinIcon(UEdGraphPin* pin);
	FString getPinURL(UEdGraphPin* pin);

	FString getDelegateIcon(UEdGraphNode* node, bool *hasDelegate=NULL);
	bool isDelegatePin(UEdGraphPin* pin);

	bool pinShouldBeVisible(UEdGraphPin* pin, TMap<FString, FString> visiblePins = TMap<FString, FString>());

	//c++/doxygen
	FString getCppType(FProperty *prop);
}
