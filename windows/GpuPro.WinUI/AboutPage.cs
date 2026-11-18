using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Media.Imaging;
namespace GpuPro.Windows;
// Aba Sobre: o projeto, links do repositório, o QR do "cafezinho" (PIX estático gerado por
// tools/pix-qr.py) e um versículo sorteado, que também aparece no rodapé da barra lateral.
internal static class Verses {
 static readonly List<(string Reference,string Text)> all=Load();
 static readonly Random random=new();
 static List<(string,string)> Load(){var list=new List<(string,string)>();try{foreach(var line in File.ReadAllLines(Path.Combine(AppContext.BaseDirectory,"Assets","versiculos.txt"))){if(line.StartsWith('#')||!line.Contains('|'))continue;var parts=line.Split('|',2);list.Add((parts[0].Trim(),parts[1].Trim()));}}catch{}if(list.Count==0)list.Add(("Salmos 23:1","O Senhor é o meu pastor; nada me faltará."));return list;}
 internal static (string Reference,string Text) Next(string? avoid=null){if(all.Count==1)return all[0];(string,string) pick;do pick=all[random.Next(all.Count)];while(pick.Item1==avoid);return pick;}
}
public sealed partial class MainWindow {
 const string Repository="https://github.com/eritongrnascimento-svg/GPU-PRO";
 (string Reference,string Text) verse=Verses.Next();
 TextBlock? paneVerse;
 // Rodapé da barra lateral, abaixo dos itens do menu.
 void ShowPaneVerse(){
  paneVerse??=new TextBlock{TextWrapping=TextWrapping.Wrap,FontSize=11,Margin=new Thickness(16,12,16,16),Opacity=.85};
  paneVerse.Inlines.Clear();paneVerse.Inlines.Add(new Microsoft.UI.Xaml.Documents.Run{Text="“"+verse.Text+"”",FontStyle=global::Windows.UI.Text.FontStyle.Italic});
  paneVerse.Inlines.Add(new Microsoft.UI.Xaml.Documents.LineBreak());paneVerse.Inlines.Add(new Microsoft.UI.Xaml.Documents.Run{Text=verse.Reference,Foreground=Accent.Fill,FontWeight=global::Microsoft.UI.Text.FontWeights.SemiBold});
  nav.PaneFooter=paneVerse;
 }
 void BuildAboutPage(){
  var about=new StackPanel{Spacing=8};
  about.Children.Add(Text("GPU-PRO 0.8.1 · Windows",18));
  about.Children.Add(Text("Monitoramento, testes, overlay e ajustes de GPU para Linux e Windows, com núcleo C++ compartilhado. Leituras vêm do driver e do sistema: o que não é exposto aparece como N/D, sem estimativas inventadas. Projeto independente, sem coleta de dados nem upload automático."));
  about.Children.Add(Text("Desenvolvido por Ériton do Nascimento · licença no arquivo LICENSE que acompanha o aplicativo.",12));
  var links=new StackPanel{Orientation=Orientation.Horizontal,Spacing=12};
  links.Children.Add(Button("Repositório no GitHub",()=>Open(Repository)));links.Children.Add(Button("Versões e downloads",()=>Open(Repository+"/releases")));links.Children.Add(Button("Relatar um problema",()=>Open(Repository+"/issues")));
  about.Children.Add(links);
  body.Children.Add(Tighten(Section("Sobre o projeto",about)));
  var coffee=new Grid{ColumnSpacing=24};coffee.ColumnDefinitions.Add(new(){Width=GridLength.Auto});coffee.ColumnDefinitions.Add(new(){Width=new GridLength(1,GridUnitType.Star)});
  var qr=new Image{Width=220,Height=220,Stretch=Stretch.Uniform};var qrPath=Path.Combine(AppContext.BaseDirectory,"Assets","pix-cafezinho.png");if(File.Exists(qrPath))qr.Source=new BitmapImage(new Uri(qrPath));
  coffee.Children.Add(new Border{Child=qr,Background=Brush(255,255,255,255),CornerRadius=new CornerRadius(10),Padding=new Thickness(6),VerticalAlignment=VerticalAlignment.Top});
  var text=new StackPanel{Spacing=10};
  text.Children.Add(Text("Aceita um cafezinho? ☕",20));
  text.Children.Add(Text("Bora tomar um café com o desenvolvedor do GPU-PRO? Só tem um detalhe: quem paga a conta é você 😄"));
  text.Children.Add(Text("Aponte a câmera do app do seu banco para o QR (PIX). Qualquer valor ajuda a manter o projeto, os testes em hardware real e as novas versões.",12));
  text.Children.Add(Text("Chave PIX: eritongrnascimento@gmail.com",12));
  var code=File.Exists(Path.Combine(AppContext.BaseDirectory,"Assets","pix-cafezinho.txt"))?File.ReadAllText(Path.Combine(AppContext.BaseDirectory,"Assets","pix-cafezinho.txt")).Trim():"";
  text.Children.Add(Button("Copiar PIX copia e cola",()=>{var data=new global::Windows.ApplicationModel.DataTransfer.DataPackage();data.SetText(code.Length>0?code:"eritongrnascimento@gmail.com");global::Windows.ApplicationModel.DataTransfer.Clipboard.SetContent(data);Record("PIX copia e cola copiado para a área de transferência.");}));
  Grid.SetColumn(text,1);coffee.Children.Add(text);
  body.Children.Add(Tighten(Section("Cafezinho",coffee)));
  var verseText=new TextBlock{TextWrapping=TextWrapping.Wrap,FontSize=17,FontStyle=global::Windows.UI.Text.FontStyle.Italic};var reference=new TextBlock{Foreground=Accent.Fill,FontWeight=global::Microsoft.UI.Text.FontWeights.SemiBold};
  void Fill(){verseText.Text="“"+verse.Text+"”";reference.Text=verse.Reference+" · Almeida Revista e Corrigida";}Fill();
  body.Children.Add(Tighten(Section("Versículo",verseText,reference,Button("Outro versículo",()=>{verse=Verses.Next(verse.Reference);Fill();ShowPaneVerse();}))));
 }
 // Abre sempre centralizado na área de trabalho do monitor principal.
 void CenterOnScreen(){
  var area=Microsoft.UI.Windowing.DisplayArea.Primary.WorkArea;var size=AppWindow.Size;
  int w=Math.Min(size.Width,area.Width),h=Math.Min(size.Height,area.Height);
  AppWindow.MoveAndResize(new global::Windows.Graphics.RectInt32(area.X+(area.Width-w)/2,area.Y+(area.Height-h)/2,w,h));
 }
}
