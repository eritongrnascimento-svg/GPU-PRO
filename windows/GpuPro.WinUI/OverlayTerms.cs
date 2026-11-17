using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Documents;
using System.Text.Json;
using System.Text.Json.Nodes;
namespace GpuPro.Windows;
// Termo de responsabilidade do GamerMonitor & Overlay (docs/AVISO-OVERLAY.md), exibido ao abrir a aba.
// O aceite fica só neste computador (termos.json) e no Log de operações.
public sealed partial class MainWindow {
 const int OverlayTermsVersion=1;
 static string TermsFile=>Path.Combine(dataRoot,"termos.json");
 static JsonObject ReadTerms(){try{if(File.Exists(TermsFile))return JsonNode.Parse(File.ReadAllText(TermsFile))?.AsObject()??new();}catch{}return new();}
 bool termsOpen;
 async Task<bool> ConfirmOverlayTerms(){
  var saved=ReadTerms();if(saved["overlay"]?["version"]?.GetValue<int>()==OverlayTermsVersion&&saved["overlay"]?["dontShow"]?.GetValue<bool>()==true)return true;
  if(termsOpen)return false;termsOpen=true;
  try{
   var path=Path.Combine(AppContext.BaseDirectory,"Assets","AVISO-OVERLAY.md");
   var text=File.Exists(path)?File.ReadAllText(path):"Termo indisponível. Reinstale o GPU-PRO.";
   var content=new StackPanel{Spacing=10};var block=new RichTextBlock{TextWrapping=TextWrapping.Wrap,IsTextSelectionEnabled=true};content.Children.Add(new ScrollViewer{Content=block,MaxHeight=430,Padding=new Thickness(0,0,14,0)});
   string title="Termo de responsabilidade";var paragraph=new List<string>();
   void Flush(){if(paragraph.Count==0)return;var p=new Paragraph{Margin=new Thickness(0,0,0,8)};AddInline(p,string.Join(' ',paragraph));block.Blocks.Add(p);paragraph.Clear();}
   foreach(var raw in text.Replace("\r","").Split('\n')){var line=raw.Trim();
    if(line.StartsWith("# ")){Flush();title=line[2..];continue;}
    if(line.StartsWith("## ")){Flush();var h=new Paragraph{Margin=new Thickness(0,6,0,4)};h.Inlines.Add(new Run{Text=line[3..],FontWeight=global::Microsoft.UI.Text.FontWeights.SemiBold,FontSize=15,Foreground=new Microsoft.UI.Xaml.Media.SolidColorBrush(theme)});block.Blocks.Add(h);continue;}
    if(line.Length==0){Flush();continue;}
    bool item=line.StartsWith("- ")||(line.Length>2&&char.IsDigit(line[0])&&line[1]=='.');
    if(item){Flush();paragraph.Add(line.StartsWith("- ")?"•  "+line[2..]:line);continue;}
    paragraph.Add(line);}
   Flush();
   var read=new CheckBox{Content="Li e aceito o termo de responsabilidade e assumo os riscos do uso"};var dontShow=new CheckBox{Content="Não exibir novamente nesta versão do termo"};
   content.Children.Add(read);content.Children.Add(dontShow);
   var dialog=new ContentDialog{XamlRoot=Root.XamlRoot,Title=title,Content=content,PrimaryButtonText="Aceitar",CloseButtonText="Recusar",DefaultButton=ContentDialogButton.Close,IsPrimaryButtonEnabled=false};
   read.Click+=(s,e)=>dialog.IsPrimaryButtonEnabled=read.IsChecked==true;
   var accepted=await dialog.ShowAsync()==ContentDialogResult.Primary&&read.IsChecked==true;
   if(accepted){saved["overlay"]=new JsonObject{["version"]=OverlayTermsVersion,["acceptedAt"]=DateTime.Now.ToString("O"),["dontShow"]=dontShow.IsChecked==true};try{Directory.CreateDirectory(dataRoot);File.WriteAllText(TermsFile,saved.ToJsonString(new JsonSerializerOptions{WriteIndented=true}));}catch{}Record($"Termo do Overlay aceito · versão {OverlayTermsVersion}"+(dontShow.IsChecked==true?" · não exibir novamente":""));}
   else Record("Termo do Overlay recusado · aba GamerMonitor & Overlay não aberta");
   return accepted;
  }finally{termsOpen=false;}
 }
 // Negrito (**texto**) e código (`texto`) simples do Markdown do termo.
 static void AddInline(Paragraph p,string text){var parts=text.Replace("`","").Split("**");for(int i=0;i<parts.Length;i++)if(parts[i].Length>0)p.Inlines.Add(new Run{Text=parts[i],FontWeight=i%2==1?global::Microsoft.UI.Text.FontWeights.SemiBold:global::Microsoft.UI.Text.FontWeights.Normal});}
}
