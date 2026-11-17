using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Windows.Foundation;
namespace GpuPro.Windows;
// Grade de cards que se ajusta à área visível da janela, como na versão Linux: escolhe o
// número de colunas e a altura dos cards para caber sem rolagem. Só quando nem o tamanho
// mínimo cabe é que a página volta a rolar.
internal sealed class FitPanel:Panel {
 internal ScrollViewer? Viewport{get;set;}
 // Fração da altura visível destinada a este painel (1 = todo o espaço abaixo do topo dele).
 internal double Share{get;set;}=1;
 // Altura reservada abaixo do painel quando Share=1 (texto de rodapé, margens).
 internal double Reserve{get;set;}=36;
 internal double MinCardWidth{get;set;}=170;
 internal double MinCardHeight{get;set;}=64;
 internal double MaxCardHeight{get;set;}=150;
 internal double Gap{get;set;}=10; int columns=1;double cardHeight;
 FrameworkElement? hooked;
 internal FitPanel(){Loaded+=(s,e)=>{Hook();InvalidateMeasure();};Unloaded+=(s,e)=>Unhook();}
 void Hook(){if(Viewport==null||hooked==Viewport)return;Unhook();hooked=Viewport;Viewport.SizeChanged+=Changed;LayoutUpdated+=Settle;}
 void Unhook(){if(hooked!=null)hooked.SizeChanged-=Changed;hooked=null;LayoutUpdated-=Settle;}
 void Changed(object s,SizeChangedEventArgs e)=>InvalidateMeasure();
 double Budget(){
  if(Viewport==null||Viewport.ViewportHeight<=0)return double.PositiveInfinity;
  if(Share<1)return Viewport.ViewportHeight*Share;
  double top=Top();var margin=(Viewport.Content as FrameworkElement)?.Margin??default;
  return Math.Max(MinCardHeight,Viewport.ViewportHeight-margin.Top-margin.Bottom-Math.Max(0,top)-Reserve);
 }
 double Top(){try{if(IsLoaded&&Viewport?.Content is UIElement content)return TransformToVisual(content).TransformPoint(new Point(0,0)).Y;}catch(ArgumentException){}return 0;}
 // A posição só é conhecida depois do primeiro layout; se mudou, mede de novo.
 double measuredTop=-1;
 void Settle(object? s,object e){var top=Top();if(Math.Abs(top-measuredTop)>2){measuredTop=top;InvalidateMeasure();}}
 protected override Size MeasureOverride(Size available){
  int n=Children.Count;if(n==0)return new Size(0,0);
  double width=double.IsInfinity(available.Width)?MinCardWidth*3:available.Width;
  int maxColumns=Math.Max(1,(int)((width+Gap)/(MinCardWidth+Gap)));double budget=Budget();bool fits=false;
  for(int c=1;c<=Math.Min(maxColumns,n);c++){int r=(n+c-1)/c;double h=(budget-(r-1)*Gap)/r;if(h>=MinCardHeight){columns=c;cardHeight=Math.Min(MaxCardHeight,h);fits=true;break;}}
  if(!fits){columns=Math.Min(maxColumns,n);cardHeight=MinCardHeight;}
  int rows=(n+columns-1)/columns;double cardWidth=(width-(columns-1)*Gap)/columns;
  foreach(var child in Children){if(child is NeonCard card)card.Fit(cardHeight);child.Measure(new Size(cardWidth,cardHeight));}
  return new Size(width,rows*cardHeight+(rows-1)*Gap);
 }
 protected override Size ArrangeOverride(Size final){
  int rows=Math.Max(1,(Children.Count+columns-1)/columns);double cardWidth=(final.Width-(columns-1)*Gap)/columns;
  for(int i=0;i<Children.Count;i++){int r=i/columns,c=i%columns;Children[i].Arrange(new Rect(c*(cardWidth+Gap),r*(cardHeight+Gap),cardWidth,cardHeight));}
  return new Size(final.Width,rows*cardHeight+(rows-1)*Gap);
 }
}
