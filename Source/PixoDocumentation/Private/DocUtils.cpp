// (c) 2023 PixoVR

#include "DocUtils.h"

#include "Misc/DefaultValueHelper.h"
#include "Misc/FileHelper.h"

#include "Internationalization/Regex.h"

#include <string>
using namespace std;

using namespace DocUtils;

FString DocUtils::htmlentities(FString in)
{
	//TArray<int> reps = { 34,39,60,61,62 };	// " ' < = >
	TArray<int> reps = { 34,39 };	// " '

	FString out;
	for (int i = 0; i < in.Len(); i++)
	{
		if (in[i] == '>')
			out += TEXT("&gt;");
		else if (in[i] == '<')
			out += TEXT("&lt;");
		//else if (in[i] == '=')
		//	out += TEXT("&equals;");
		else if (in[i] == '&')
			out += TEXT("&amp;");
		else if (in[i] == '"')			//34
			out += TEXT("&quot;");
		else if (reps.Contains(in[i]))
			out += FString::Printf(TEXT("&#%d;"), (int)in[i]);
		else if (in[i] > 127)
			out += FString::Printf(TEXT("&#%d;"), (int)in[i]);
		else
			out += in[i];
	}

	return out;
}

//https://vocal.com/video/rgb-and-hsvhsihsl-color-space-conversion/

void DocUtils::RGBtoHSL(float R, float G, float B, float* h, float* s, float* l)
{
	float Cmax = fmax(R, fmax(G, B));
	float Cmin = fmin(R, fmin(G, B));
	float D = Cmax - Cmin;
	float epsilon = 0.0001;

	float H;
	if (D < epsilon)
		H = 0.0f;
	else if (Cmax == R)
		H = 60.0f * fmod(((G - B) / D), 6.0f);
	else if (Cmax == G)
		H = 60.0f * (2.0f + ((B - R) / D));
	else //if (Cmax == B)
		H = 60.0f * (4.0f + ((R - G) / D));

	float L = (Cmax + Cmin) / 2.0f;
	float S = (D < epsilon) ? 0.0f : D / (1-fabs(2.0f*L - 1.0f));

	*h = H;
	*s = S;
	*l = L;
}

void DocUtils::HSLtoRGB(float H, float S, float L, float* r, float* g, float* b)
{
	float c = (1.0f - fabs(2.0f * L - 1)) * S;
	float x = c * (1 - fabs(fmod(H/60.0f, 2.0f) - 1.0f));
	float m = L - c / 2.0f;

	float rp, gp, bp;
	if (H < 60.0f)
	{		rp = c;	gp = x; bp = 0;		}
	else if (H < 120.0f)
	{		rp = x;	gp = c; bp = 0;		}
	else if (H < 180.0f)
	{		rp = 0;	gp = c; bp = x;		}
	else if (H < 240.0f)
	{		rp = 0;	gp = x; bp = c;		}
	else if (H < 300.0f)
	{		rp = x;	gp = 0; bp = c;		}
	else
	{		rp = c;	gp = 0; bp = x;		}

	*r = rp + m;
	*g = gp + m;
	*b = bp + m;
}

FString DocUtils::createColorString(FLinearColor color, float alpha, float exponent)
{
	//return "#"+color.ToFColor(true).ToHex();

	float R, G, B;
	float H, S, L;

	R = color.R;
	G = color.G;
	B = color.B;

//	printf("in: %0.2f %0.2f %0.2f ", R, G, B);

	RGBtoHSL(R, G, B, &H, &S, &L);

//	printf("hsl: %0.2f %0.2f %0.2f ", H, S, L);

	H = fmod(H + 180.0f, 360.0f);	// our hue rotation for the site

	//L = powf(L, 2.2f);			// to sRGB
	L = powf(L, 1/exponent);		// our gamma correct

	HSLtoRGB(H, S, L, &R, &G, &B);

	FLinearColor rgb(R, G, B, alpha);

//	printf("rgb: %0.2f %0.2f %0.2f ", rgb.R, rgb.G, rgb.B);
//	printf("\n");

	rgb = rgb.GetClamped();

	FString c = "#" + rgb.ToFColor(true).ToHex();

	if (c == "#0000FF" || c == "#0000FFFF")		//so, it's fudged.
		return "#9999FF";

	return c;
}

FString DocUtils::createVariableName(FString name)
{
	std::string g(TCHAR_TO_UTF8(*name));

	//std::replace_if(g.begin(), g.end(), isNotAlphaNum, ' ');
	for (auto& c : g)
		if (!isalnum(c)) c = '_';	//replace non-alnum with '_'

	FString variableName(g.c_str());

	return variableName;
}

FString DocUtils::getClassName(UClass* _class)
{
	FString className = _class->GetPrefixCPP() + _class->GetFName().ToString();
	return className;
}

void DocUtils::addAllGraphs(TArray<UEdGraph*>& container, TArray<UEdGraph*>& graphs)
{
	for (UEdGraph* g : graphs)
	{
		container.Add(g);
		addAllGraphs(container, g->SubGraphs);
	}
}

FString DocUtils::getTrimmedConfigFilePath(FString path)
{
	FString confdir = FPaths::ProjectConfigDir();
	confdir.RemoveFromEnd("/");
	confdir = FPaths::GetPath(confdir)+"/";

	FPaths::NormalizeDirectoryName(confdir);
	FPaths::NormalizeFilename(path);

	FString trimmed = path;
	trimmed.RemoveFromStart(confdir);
	trimmed.RemoveFromStart("/");

	//wcout << "TRIMMED: " << *trimmed << " || " << *confdir << " || " << *path << endl;

	return trimmed;
}

FString DocUtils::prepTemplateString(FString prefix, vmap style, FString string)
{
	FString h = prefix + string;

	for (auto& e : style)
		h = h.Replace(*e.Key, *e.Value);

	//do it twice, in case a value also has variables in it.
	for (auto& e : style)
		h = h.Replace(*e.Key, *e.Value);

	int i = 2;
	while (i--)		//do it a few times to get all of them
	{
		//clean up any empty <i> tags from replacement
		h = h.Replace(TEXT("<br/>&nbsp;&nbsp;&nbsp;&nbsp;<i></i>"), TEXT(""));	//for nodes missing a second line
		h = h.Replace(TEXT("<br/>&nbsp;<i></i>"), TEXT(""));							//for nodes missing a second line
		h = h.Replace(TEXT("<br/><i></i>"), TEXT(""));									//for nodes missing a second line
		h = h.Replace(TEXT("<b></b>"), TEXT(""));

		h = h.Replace(TEXT("\r"), *(TEXT("") + prefix));
		h = h.Replace(TEXT("\n"), *(TEXT("\n") + prefix));
		//h = h.Replace(TEXT("\\"), *(TEXT("\\\\") + prefix));

			//replace empty font tag
		FRegexPattern emptyFont(TEXT("<font [^<]*><\\/font>"));
		FRegexMatcher matcher(emptyFont, h);
		TArray<FString> m;
		while (matcher.FindNext())
		{
			FString s = matcher.GetCaptureGroup(0);
			//wcout << "MATCH: [" << *s << "]" << endl;
			m.Add(s);
		}
		for (FString s : m)
			h = h.Replace(*s, TEXT(""));
	}

	return h;
}

NodeType DocUtils::getNodeType(UEdGraphNode* node, NodeType defaultType)
{
	FString stype = node->GetClass()->GetFName().ToString();
	NodeType type = NodeType_e.FindOrAdd(stype, defaultType);

	UK2Node* n2 = dynamic_cast<UK2Node*>(node);
	if (n2 && n2->ShouldDrawCompact())
		return NodeType::compact;

	//if (FBlueprintEditorUtils::IsTunnelInstanceNode(node))			//is this needed?
	//	return NodeType::tunnel;

	if (type == NodeType::none)
	{
		UE_LOG(LOG_DOT, Error, TEXT("Unknown node type: '%s'. Returning NodeType::none."), *stype);
	}

	return type;
}

FString DocUtils::getNodeTypeGroup(NodeType type)
{
	return *NodeType_e.FindKey(type);
}


FString DocUtils::getNodeTemplate(NodeType type, bool hasDelegate)
{
	//magic node types:
//	(all types, except those noted below) // regular node
//	EdGraphNode_Comment		// actually has size
//	[event thing]			// will have different urls
//	K2Node_Knot				// ShouldDrawNodeAsControlPointOnly = true
//	K2Node_Tunnel			// input or output
// 	K2Node_VariableGet		// variable, no header, tinted node
//	K2Node_Composite		// subgraph, will have different urls
//	[function]				// subgraph!
//	[macro]					// subgraph, but probably not openable
//	material node(s)		// no icon, collapsable?

	FString t;
	switch (type)
	{
	case NodeType::materialcomposite:
	case NodeType::composite:			//composite	//no icon, no header color, may be multi-line title
		t = R"LONGRAW(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table style="rounded" bgcolor='_HEADERCOLORLIGHT_' border="1" cellborder="0" cellspacing="0" cellpadding="_COMPOSITEPADDING_">
				<tr>
					<td align="left" balign="left" colspan="4"><font color="_HEADERTEXTCOLOR_"><b>_NODETITLE_</b><br/><i>_NODETITLE2_</i></b></td>
				</tr>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)LONGRAW";

		break;

	case NodeType::compact:		//compact
		t = R"LONGRAW(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table style="rounded" align="left" bgcolor='_HEADERCOLORLIGHT_' border="1" cellborder="0" cellspacing="0" cellpadding="_COMPOSITEPADDING_">
				<tr>
					<td align="center" balign="center" fixedsize="true" width="_COMPACTWIDTH_" height="1" colspan="4"><font color="_COMPACTCOLOR_" point-size="_COMPACTSIZE_"><b>_NODETITLE_</b></font></td>
				</tr>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)LONGRAW";

		break;

	case NodeType::materialtunnel:
	case NodeType::materialnode:
		t = R"LONGRAW(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table bgcolor='_NODECOLOR_' border="1" cellborder="0" cellspacing="0" cellpadding="_CELLPADDING_">
				<tr>
					<td colspan="4" align="left"  balign="left"  width="1" height="0" bgcolor="_HEADERCOLORDIM_" port="icon"    ><font color="_HEADERTEXTCOLOR_"><b>&nbsp;_NODETITLE__HEIGHTSPACER_&nbsp;</b><br/>&nbsp;<i>_NODETITLE2_</i></font></td>
				</tr>
				<hr/>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)LONGRAW";

		break;

	case NodeType::event:		//event
	case NodeType::macro:		//macro		//dimmer header
	case NodeType::node:		//plain node

		if (hasDelegate)
		{
			t = R"LONGRAW(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table bgcolor='_NODECOLOR_' border="1" cellborder="0" cellspacing="0" cellpadding="_CELLPADDING_">
				<tr>
					<td colspan="3" align="left"  balign="left"  width="1" height="0" bgcolor="_HEADERCOLORDIM_" port="icon"    ><font color="_HEADERTEXTCOLOR_"><b>_NODEICON__HEIGHTSPACER__NODETITLE__HEIGHTSPACER_&nbsp;</b><br/>&nbsp;&nbsp;&nbsp;&nbsp;<i>_NODETITLE2_</i></font></td>
					<td colspan="1" align="right" balign="right" width="1" height="0" bgcolor="_HEADERCOLORDIM_" port="delegate"><font color="_HEADERTEXTCOLOR_">_HEIGHTSPACER__NODEDELEGATE_</font></td>
				</tr>
				<hr/>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)LONGRAW";
		}
		else
		{
			t = R"LONGRAW(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table bgcolor='_NODECOLOR_' border="1" cellborder="0" cellspacing="0" cellpadding="_CELLPADDING_">
				<tr>
					<td colspan="4" align="left"  balign="left"  width="1" height="0" bgcolor="_HEADERCOLORDIM_" port="icon"    ><font color="_HEADERTEXTCOLOR_"><b>_NODEICON__HEIGHTSPACER__NODETITLE__HEIGHTSPACER_&nbsp;</b><br/>&nbsp;&nbsp;&nbsp;&nbsp;<i>_NODETITLE2_</i></font></td>
				</tr>
				<hr/>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)LONGRAW";
		}

		break;

	case NodeType::spawn:
	case NodeType::function:
	case NodeType::tunnel:

		if (hasDelegate)
		{
			t = R"LONGRAW(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table bgcolor='_NODECOLOR_' border="1" cellborder="0" cellspacing="0" cellpadding="_CELLPADDING_">
				<tr>
					<td colspan="3" align="left"  balign="left"  width="1" height="0" bgcolor="_HEADERCOLOR_" port="icon"    ><font color="_HEADERTEXTCOLOR_"><b>_NODEICON__HEIGHTSPACER__NODETITLE__HEIGHTSPACER_&nbsp;</b><br/>&nbsp;&nbsp;&nbsp;&nbsp;<i>_NODETITLE2_</i></font></td>
					<td colspan="1" align="right" balign="right" width="1" height="0" bgcolor="_HEADERCOLOR_" port="delegate"><font color="_HEADERTEXTCOLOR_">_HEIGHTSPACER__NODEDELEGATE_</font></td>
				</tr>
				<hr/>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)LONGRAW";
		}
		else
		{
			t = R"LONGRAW(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table bgcolor='_NODECOLOR_' border="1" cellborder="0" cellspacing="0" cellpadding="_CELLPADDING_">
				<tr>
					<td colspan="4" align="left"  balign="left"  width="1" height="0" bgcolor="_HEADERCOLOR_" port="icon"    ><font color="_HEADERTEXTCOLOR_"><b>_NODEICON__HEIGHTSPACER__NODETITLE__HEIGHTSPACER_&nbsp;</b><br/>&nbsp;&nbsp;&nbsp;&nbsp;<i>_NODETITLE2_</i></font></td>
				</tr>
				<hr/>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)LONGRAW";
		}

		break;

	case NodeType::comment:		//comment node
		t = R"LONGRAW(
_NODENAME_ [
	layer="comments"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table bgcolor="_HEADERCOLORTRANS_" border="1" cellborder="0" cellspacing="0" cellpadding="_CELLPADDING_" width="_WIDTH_" height="_HEIGHT_">
				<tr>
					<td align="left" balign="left" height="0" width="_WIDTH_" bgcolor="_HEADERCOLOR_" port="comment"><font color="_HEADERTEXTCOLOR_" point-size="_FONTSIZECOMMENT_"><b>_NODECOMMENT_</b></font></td>
				</tr>
				<hr/>
				<tr>
					<td colspan="3" align="left" port="body"></td>
				</tr>
			</table>
		</td></tr></table>
	>
];)LONGRAW";

		break;

	case NodeType::variable:	//variable node
		t = R"LONGRAW(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table align="right" style="rounded" bgcolor='_HEADERCOLORLIGHT_' border="1" cellborder="0" cellspacing="0" cellpadding="_CELLPADDING_">
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)LONGRAW";

		break;

	case NodeType::variableset:		//variableset
		t = R"LONGRAW(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"
	URL="_URL_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table style="rounded" align="left" bgcolor='_HEADERCOLORLIGHT_' border="1" cellborder="0" cellspacing="0" cellpadding="_COMPOSITEPADDING_">
				<tr>
					<td align="center" balign="center" fixedsize="true" width="_COMPACTWIDTH_" height="1" colspan="4"><font color="_COMPACTCOLOR_" point-size="_COMPACTSIZE_"><b>SET</b></font></td>
				</tr>
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)LONGRAW";

		break;

	case NodeType::bubble:		//bubble node

		t = R"LONGRAW(
_NODENAME__comment [
	layer=bubbles
	pos="_POS_"
	tooltip="_TOOLTIP_"
	fontcolor="_FONTCOLORBUBBLE_"
	class="_CLASS_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table style="rounded" bgcolor="_NODECOLORTRANS_" border="1" cellborder="0" cellspacing="0" cellpadding="_COMMENTPADDING_">
				<tr>
					<td align="left" balign="left" port="comment"><font point-size="_FONTSIZEBUBBLE_">_NODECOMMENT_</font>&nbsp;</td>
				</tr>
				<tr>
					<td align="left" cellpadding="0" cellspacing="0" fixedsize="true" width="40" height="1"><font color="_BORDERCOLOR_" point-size="11">&nbsp;&nbsp;&nbsp;&nbsp;&#9700;</font></td>
				</tr>
			</table>
		</td></tr></table>
	>
];)LONGRAW";

		break;

	case NodeType::route:		//route node
		t = R"LONGRAW(
_NODENAME_ [
	layer="nodes"
	pos="_POS_"
	tooltip="_TOOLTIP_"
	class="_CLASS_"

	label=<
		<table fixedsize="true" border="0" width="1" height="1" cellborder="0" cellspacing="0" cellpadding="0"><tr><td>
			<table border="0" cellborder="0" cellspacing="0" cellpadding="0">
_PORTROWS_
			</table>
		</td></tr></table>
	>
];)LONGRAW";

		break;

	default:
		//wcout << *FString::Printf(TEXT("//Unknown node type: '%d'. Can't create output."), type) << endl;
		return FString::Printf(TEXT("comment=\"Unknown node type: '%d'. Can't create output.\""), type);

		break;
	}

	return t;
}

FString DocUtils::getNodeTooltip(UEdGraphNode* node)
{
	FString tooltip = node->GetTooltipText().ToString();
	tooltip = tooltip.Replace(TEXT("\\"), TEXT("\\\\"));
	tooltip = tooltip.ReplaceQuotesWithEscapedQuotes();
	tooltip = tooltip.TrimStartAndEnd();
	tooltip = htmlentities(tooltip);
	//tooltip = tooltip.Replace(TEXT("\n"), TEXT("<BR/>"));
	//tooltip = tooltip.Replace(TEXT("\n"), TEXT("\\n"));			//this tooltip is never in html context
	//tooltip = tooltip.Replace(TEXT("\n"), TEXT("&#10;"));
	tooltip = tooltip.Replace(TEXT("\r"), TEXT(""));
	tooltip = tooltip.Replace(TEXT("\n"), TEXT("&#013;"));
	//tooltip = tooltip.Replace(TEXT("&gt;"), TEXT(">"));
	//tooltip = tooltip.Replace(TEXT("&lt;"), TEXT("<"));
	//tooltip = tooltip.Replace(TEXT("\\"), TEXT("\\\\"));

	return tooltip;
}

FString DocUtils::getNodeIcon(UEdGraphNode* node)
{
	NodeType type = getNodeType(node);

	switch (type)
	{
		case NodeType::event:
			return NodeIcons["event"];		//event

		case NodeType::materialcomposite:
		case NodeType::composite:
		case NodeType::function:
			return NodeIcons["container"];	//container

		case NodeType::macro:
			return NodeIcons["switch"];		//switch

		case NodeType::path:
			return NodeIcons["path"];		//path

		case NodeType::tunnel:
			return NodeIcons["io"];			//arrows

		case NodeType::spawn:
			return NodeIcons["spawn"];		//star

		//spawn

		case NodeType::route:
		case NodeType::variable:
		case NodeType::variableset:
		case NodeType::comment:
		case NodeType::bubble:
			return "";						//no icon needed

		default:
			return NodeIcons["default"];	//the function icon
			break;
	}
}

FString DocUtils::getDelegateIcon(UEdGraphNode* node, bool *hasDelegate)
{
	const TArray< UEdGraphPin* > pins = node->Pins;
	//const TArray< UEdGraphPin* > pins = n->GetAllPins();

	*hasDelegate = false;

	for (UEdGraphPin* p : pins)
	{
		if (isDelegatePin(p))
		{
			*hasDelegate = true;

			FString dc = getPinColor(p);
			if (p->HasAnyConnections())
				return FString::Printf(TEXT("<font color=\"%s\">%s</font>"), *dc, *PinIcons["delegate"].Value);
			else
				return FString::Printf(TEXT("<font color=\"%s\">%s</font>"), *dc, *PinIcons["delegate"].Key);
		}
	}

	return "&nbsp;";
}

bool DocUtils::isDelegatePin(UEdGraphPin* pin)
{
	if (pin->Direction == EEdGraphPinDirection::EGPD_Output)
	{	//if (p->PinType == FEdGraphPinType::
		if (getPinType(pin) == "Delegate")
			return true;
	}

	return false;
}

FString DocUtils::getPinLabel(UEdGraphPin* p)
{
	FString n;

	//FString type = getPinType(p);
	//if (type == "Exec")	return "";

	n = p->GetOwningNode()->GetPinDisplayName(p).ToString();

	if (n.IsEmpty())	n = p->GetDisplayName().ToString();
	if (n.IsEmpty())	n = p->PinFriendlyName.ToString();
	if (n.IsEmpty())	n = p->PinName.ToString();
	if (n.IsEmpty())	n = p->GetName();

	if (n.ToLower() == "exec" ||
		n.ToLower() == "execute" ||
		n.ToLower() == "then")
		return " ";	//must be ' ' because of getPinTooltip and prepNodePortRows.  <font></font> can't be empty!

	return n;
}

FString DocUtils::getPinTooltip(UEdGraphPin* p, TMap<FString,FString>visiblePins)
{
	FString hover;
	UEdGraphNode* n = p->GetOwningNode();
	n->GetPinHoverText(*p, hover);

	if (visiblePins.Num())
	{
		FString nm = getPinLabel(p);
		if (visiblePins.Contains(nm))
			hover = visiblePins[nm];
	}

	hover = hover.TrimStartAndEnd();
	hover = htmlentities(hover);
	hover = hover.Replace(TEXT("\r"), TEXT(""));
	hover = hover.Replace(TEXT("\n"), TEXT("&#013;"));
	//hover = hover.Replace(TEXT("\\"), TEXT("\\\\"));
	//hover = hover.Replace(TEXT("\""), TEXT("\\\""));
	return hover;
}

FString DocUtils::getPinType(UEdGraphPin* pin, bool useSchema)
{
	FEdGraphPinType y = pin->PinType;

	//FBlueprintEditorUtils::GetTypeForPin(*pin);

	//FString t = FString::Printf(
	//	TEXT("%s/%s"),
	//	*y.PinCategory.ToString(),
	//	*y.PinSubCategory.ToString()
	//);

	if (useSchema)
	{
		const UEdGraphSchema* Schema = pin->GetSchema();
		const UEdGraphSchema_K2* s = dynamic_cast<const UEdGraphSchema_K2*>(Schema);

		if (s)
		{
			//s->GetVariableTypeTree
			//s->GetAllMarks
			return s->GetCategoryText(pin->PinType.PinCategory).ToString();
		}
	}

	if (y.IsArray())
		return "array";

	if (y.IsMap())
		return "map";

	if (y.IsSet())
		return "set";

	if (y.IsContainer())			//no idea what a "container" is
		return "container";

	FString t = y.PinCategory.ToString();
	return FName::NameToDisplayString(t, false);
}

FString DocUtils::getPinURL(UEdGraphPin* pin)
{
	FString url="";

	//do we ever actually need a pinurl?
	//FString url = getNodeURL(pin->GetOwningNode(),pin->Direction);

	return url;
}

// straight up copied from SGraphNode.cpp
bool DocUtils::pinShouldBeVisible(UEdGraphPin* InPin, TMap<FString, FString> visiblePins)
{
	if (visiblePins.Num())
	{
		FString nm = getPinLabel(InPin);
		if (!visiblePins.Contains(nm))
			return false;
	}

	bool bHideNoConnectionPins = false;
	bool bHideNoConnectionNoDefaultPins = false;

	// Not allowed to hide exec pins
	const bool bCanHidePin = (InPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec);

	//if (OwnerGraphPanelPtr.IsValid() && bCanHidePin)
	//{
		//bHideNoConnectionPins = OwnerGraphPanelPtr.Pin()->GetPinVisibility() == SGraphEditor::Pin_HideNoConnection;
		//bHideNoConnectionNoDefaultPins = OwnerGraphPanelPtr.Pin()->GetPinVisibility() == SGraphEditor::Pin_HideNoConnectionNoDefault;
	//}

	const bool bIsOutputPin = InPin->Direction == EGPD_Output;
	const bool bPinHasDefaultValue = !InPin->DefaultValue.IsEmpty() || (InPin->DefaultObject != NULL);
	const bool bIsSelfTarget = (InPin->PinType.PinCategory == UEdGraphSchema_K2::PC_Object) && (InPin->PinName == UEdGraphSchema_K2::PN_Self);
	const bool bPinHasValidDefault = !bIsOutputPin && (bPinHasDefaultValue || bIsSelfTarget);
	const bool bPinHasConections = InPin->LinkedTo.Num() > 0;

	const bool bPinDesiresToBeHidden =
		   InPin->bHidden
		|| (bHideNoConnectionPins && !bPinHasConections)
		|| (bHideNoConnectionNoDefaultPins && !bPinHasConections && !bPinHasValidDefault);

	// No matter how strong the desire, a pin with connections can never be hidden!
	const bool bShowPin = !bPinDesiresToBeHidden || bPinHasConections;

	if (InPin->bAdvancedView && InPin->GetOwningNode()->AdvancedPinDisplay == ENodeAdvancedPins::Hidden)
		return false;

	return bShowPin;
}

FString DocUtils::getPinPort(UEdGraphPin* p)
{
	UEdGraphNode* n = p->GetOwningNode();
	bool isRoute = getNodeType(n, NodeType::node) == NodeType::route;

	if (isRoute)			return "port";
	if (isDelegatePin(p))	return "delegate";

	return "P_"+p->PinId.ToString();
}

FString DocUtils::getPinDefaultValue(UEdGraphPin* pin)
{
	FString v = pin->DefaultValue;

	//TODO: get proper default values!

	//FEdGraphPinType pt = pin->PinType;
	//FEdGraphPinType::StaticStruct;

	/*
	if (pin->DefaultObject)
	{
		FJsonSerializableKeyValueMap vmap;
		pin->DefaultObject->GetNativePropertyValues(vmap);

		wcout << "def: " << *getPinLabel(pin) << endl;
		for (TTuple<FString, FString> vv : vmap)
		{
			wcout << "key: " << *vv.Key << " || " << *vv.Value << endl;
		}
	}
	*/

	//output is always hidden
	if (pin->Direction == EEdGraphPinDirection::EGPD_Output)
		return "";

	//bool
	if (v == "true")
		return "<font color=\"_VALCOLOR_\">&#9746;</font>";	//checkbox
	if (v == "false")
		return "<font color=\"_VALCOLOR_\">&#9744;</font>";	//checkbox

	//FString tmpl;
	//tmpl = "<table border=\"1\" cellpadding=\"1\" bgcolor=\"_VALCOLOR_\"><tr><td>%0.2f</td></tr></table>";

	if (FDefaultValueHelper::IsStringValidFloat(v))
	{
		float f;
		if (FDefaultValueHelper::ParseFloat(v, f))
			return FString::Printf(TEXT("<font color=\"_VALCOLOR_\">[%0.3f]</font>"), f);
		else
			//convert failed, not much else to do.
			return FString::Printf(TEXT("<font color=\"_VALCOLOR_\">[float]</font>"));
	}

	if (FDefaultValueHelper::IsStringValidInteger(v))
	{
		int i;
		if (FDefaultValueHelper::ParseInt(v, i))
			return FString::Printf(TEXT("<font color=\"_VALCOLOR_\">[%d]</font>"), i);
		else
			//convert failed, not much else to do.
			return FString::Printf(TEXT("<font color=\"_VALCOLOR_\">[int]</font>"));
	}

	FString cicon = "&#9724;";

	if (FDefaultValueHelper::IsStringValidLinearColor(v))
	{
		FLinearColor c;
		if (FDefaultValueHelper::ParseLinearColor(v, c))
		{
			return FString::Printf(TEXT("<font color=\"%s\">%s</font>"), *createColorString(c), *cicon);
			//return FString::Printf(TEXT("<font color=\"%0.2f %0.2f %0.2f\">%s</font>"), c.R, c.G, c.B, *cicon);
		}
		else
			//convert failed, not much else to do.
			return FString::Printf(TEXT("<font color=\"red\">%s</font>"),*cicon);
	}

	float r, g, b, a;
	if (swscanf(TCHAR_TO_WCHAR(*v), L"(R=%f,G=%f,B=%f,A=%f)", &r, &g, &b, &a) == 4)
	{
		FLinearColor c(r, g, b, a);
		return FString::Printf(TEXT("<font color=\"%s\">%s</font>"), *createColorString(c), *cicon);
		//return FString::Printf(TEXT("<font color=\"%0.2f %0.2f %0.2f %0.2f\">%s</font>"), r, g, b, a, *cicon);
	}

	if (swscanf(TCHAR_TO_WCHAR(*v), L"(R=%f,G=%f,B=%f)", &r, &g, &b) == 3)
	{
		FLinearColor c(r, g, b);
		return FString::Printf(TEXT("<font color=\"%s\">%s</font>"), *createColorString(c), *cicon);
		//return FString::Printf(TEXT("<font color=\"%0.2f %0.2f %0.2f\">%s</font>"), r, g, b, *cicon);
	}

	//pin->DefaultObject
	//FDefaultValueHelper::

	//TODO
	//rotator
	//vector
	//vector2d
	//vector4
	//Fcolor (already have FLinearColor)

	//default output
	v = v.TrimStartAndEnd();
	v = v.Replace(TEXT("\r"), TEXT(""));
	v = v.Replace(TEXT("\n"), TEXT("&#013;"));
	if (v.IsEmpty())
		return "";		//presumably a label or icon exists, so empty is ok.

	return FString::Printf(TEXT("<font color=\"_VALCOLOR_\">[%s]</font>"), *v);
}

FString DocUtils::getCppType(FProperty *prop)
{
	TArray<FString> containers{ "TMap","TSet","TArray" };

	FString extype;
	FString ctype = prop->GetCPPType();
	FString mtype = prop->GetCPPMacroType(extype);
	FString type = ctype;

	if (containers.Contains(ctype))
		type = FString::Printf(TEXT("%s<%s>"), *ctype, *extype);

	//TODO: macro type!

	//TODO: FUNCTION type!

	return type;
}

FString DocUtils::getPinColor(UEdGraphPin* pin)
{
	FLinearColor c = FLinearColor::Black;

	if (pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Delegate)
	{
		//c = FLinearColor::Red*.5;
		return createColorString(FLinearColor::Red,1.0f,1.5f);
	}
	else if (pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
	{
		if (const UEdGraphSchema* Schema = pin->GetSchema())
			c = Schema->GetPinTypeColor(pin->PinType);
	}
	else
	{
		//c = FLinearColor::Black;
		c = FLinearColor(0.2f, 0.2f, 0.2f);
	}

	// this happens in materials
	if (c == FLinearColor::White)		//our color scheme doesn't like white
		c *= .2;

	return createColorString(c);
}

FString DocUtils::getPinIcon(UEdGraphPin* pin)
{
	bool isConnected = pin->HasAnyConnections();
	FString type = getPinType(pin);

	if (type == "Exec")
		return isConnected ? PinIcons["exec"].Value : PinIcons["exec"].Key;
	else if (type == "Delegate")
		return isConnected ? PinIcons["delegate"].Value : PinIcons["delegate"].Key;
	else if (PinIcons.Contains(type))
		return isConnected ? PinIcons[type].Value : PinIcons[type].Key;

	//Object
	//Real
	//Struct
	//Wildcard
	//Bool
	//String
	//LinearColor
	//...?

	return isConnected ? PinIcons["data"].Value : PinIcons["data"].Key;
}
