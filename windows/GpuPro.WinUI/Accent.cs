using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Media;
namespace GpuPro.Windows;

// Cor de destaque do aplicativo pelo fabricante da GPU selecionada, como na versão Linux:
// NVIDIA verde, AMD vermelho, Intel azul. Os pincéis são compartilhados e substituem os de destaque
// do WinUI (que seguiriam a cor de destaque do Windows); trocar a cor atualiza tudo na hora.
internal static class Accent {
 internal static readonly SolidColorBrush Fill=new(),Secondary=new(),Tertiary=new(),Text=new(),TextSecondary=new(),TextTertiary=new();
 internal static global::Windows.UI.Color Current{get;private set;}
 static readonly global::Windows.UI.Color nvidia=C(118,185,0),amd=C(237,28,36),intel=C(22,140,232),other=C(153,153,153);
 static global::Windows.UI.Color C(byte r,byte g,byte b)=>global::Windows.UI.Color.FromArgb(255,r,g,b);

 internal static global::Windows.UI.Color ForGpu(string? vendor,string? name){
  string v=(vendor??"")+" "+(name??"");
  if(v.Contains("NVIDIA",StringComparison.OrdinalIgnoreCase)||v.Contains("GeForce",StringComparison.OrdinalIgnoreCase)||v.Contains("Quadro",StringComparison.OrdinalIgnoreCase))return nvidia;
  if(v.Contains("AMD",StringComparison.OrdinalIgnoreCase)||v.Contains("Radeon",StringComparison.OrdinalIgnoreCase)||v.Contains("ATI ",StringComparison.OrdinalIgnoreCase))return amd;
  if(v.Contains("Intel",StringComparison.OrdinalIgnoreCase)||v.Contains("Arc",StringComparison.Ordinal))return intel;
  return other;
 }

 // Substitui os pincéis de destaque do WinUI (que seguiriam a cor do Windows) por pincéis próprios.
 // Deve ser chamado no OnLaunched, logo antes de criar a janela: pincéis criados no construtor do
 // App, antes da primeira janela, derrubam o WinUI ao renderizar.
 static readonly List<(SolidColorBrush Brush,int Role)> overrides=new();
 internal static void Install(ResourceDictionary resources){
  var keys=new (string,int)[]{("AccentFillColorDefaultBrush",0),("AccentFillColorSecondaryBrush",1),("AccentFillColorTertiaryBrush",2),
   ("AccentTextFillColorPrimaryBrush",3),("AccentTextFillColorSecondaryBrush",4),("AccentTextFillColorTertiaryBrush",5),
   ("NavigationViewSelectionIndicatorForeground",0),("SystemControlHighlightAccentBrush",0),("SystemControlForegroundAccentBrush",0),
   ("SystemControlBackgroundAccentBrush",0),
   // Chaves de cada controle: no WinUI elas apontam para os pincéis de destaque internos, não para os acima.
   ("PivotHeaderItemSelectedPipeFill",0),("ListViewItemSelectionIndicatorBrush",0),("ComboBoxItemPillFillBrush",0),
   ("CheckBoxCheckBackgroundFillChecked",0),("CheckBoxCheckBackgroundFillCheckedPointerOver",1),("CheckBoxCheckBackgroundFillCheckedPressed",2),
   ("CheckBoxCheckBackgroundStrokeChecked",0),("CheckBoxCheckBackgroundStrokeCheckedPointerOver",1),("CheckBoxCheckBackgroundStrokeCheckedPressed",2),
   ("CheckBoxCheckBackgroundFillIndeterminate",0),("CheckBoxCheckBackgroundFillIndeterminatePointerOver",1),("CheckBoxCheckBackgroundFillIndeterminatePressed",2),
   ("ToggleSwitchFillOn",0),("ToggleSwitchFillOnPointerOver",1),("ToggleSwitchFillOnPressed",2),("ToggleSwitchStrokeOn",0),
   ("SliderTrackValueFill",0),("SliderTrackValueFillPointerOver",1),("SliderTrackValueFillPressed",2),
   ("SliderThumbBackground",0),("SliderThumbBackgroundPointerOver",1),("SliderThumbBackgroundPressed",2),
   ("ProgressBarForeground",0),("ProgressRingForegroundThemeBrush",0),("RadioButtonOuterEllipseCheckedFill",0),
   ("RadioButtonOuterEllipseCheckedFillPointerOver",1),("RadioButtonOuterEllipseCheckedFillPressed",2),
   ("TextControlBorderBrushFocused",0),("TextControlSelectionHighlightColor",0),("HyperlinkButtonForeground",3),("HyperlinkButtonForegroundPointerOver",4),
   ("AccentButtonBackground",0),("AccentButtonBackgroundPointerOver",1),("AccentButtonBackgroundPressed",2)};
  foreach(var (key,role) in keys){var brush=new SolidColorBrush();overrides.Add((brush,role));resources[key]=brush;}
  Set(nvidia);
 }

 // Retorna verdadeiro quando a cor mudou.
 internal static bool Set(global::Windows.UI.Color color){
  if(color==Current&&Fill.Color==color)return false;
  Current=color;
  static global::Windows.UI.Color Mix(global::Windows.UI.Color c,double white,byte alpha=255)=>global::Windows.UI.Color.FromArgb(alpha,(byte)(c.R+(255-c.R)*white),(byte)(c.G+(255-c.G)*white),(byte)(c.B+(255-c.B)*white));
  var roles=new[]{color,Mix(color,0,230),Mix(color,0,204),Mix(color,.25),Mix(color,.4),Mix(color,.15)};
  Fill.Color=roles[0];Secondary.Color=roles[1];Tertiary.Color=roles[2];Text.Color=roles[3];TextSecondary.Color=roles[4];TextTertiary.Color=roles[5];
  foreach(var (brush,role) in overrides)brush.Color=roles[role];
  return true;
 }
}
