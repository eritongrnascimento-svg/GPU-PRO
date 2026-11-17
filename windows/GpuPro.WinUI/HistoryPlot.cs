using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Shapes;
namespace GpuPro.Windows;
internal sealed class HistoryPlot:Grid {
 readonly Canvas canvas=new();readonly TextBlock heading=new(){Margin=new Thickness(12,8,12,0),FontSize=12};double[] data=Array.Empty<double>();global::Windows.UI.Color accent;
 internal HistoryPlot(string caption){Height=160;Background=new SolidColorBrush(global::Windows.UI.Color.FromArgb(210,17,17,21));Children.Add(canvas);Children.Add(heading);heading.Text=caption;Tag=caption;SizeChanged+=(s,e)=>Draw();}
 internal void Update(IEnumerable<double> samples,global::Windows.UI.Color color){data=samples.Where(double.IsFinite).TakeLast(240).ToArray();accent=color;Draw();}
 void Draw(){canvas.Children.Clear();double w=ActualWidth,h=ActualHeight;if(w<20||h<45)return;for(int i=1;i<5;i++)canvas.Children.Add(new Line{X1=0,Y1=h*i/5,X2=w,Y2=h*i/5,Stroke=new SolidColorBrush(global::Windows.UI.Color.FromArgb(255,37,37,43)),StrokeThickness=1});heading.Text=Tag+(data.Length<2?" · aguardando dados":"");if(data.Length<2)return;double max=Math.Max(1,data.Max()*1.2);var line=new Polyline{Stroke=new SolidColorBrush(accent),StrokeThickness=2};for(int i=0;i<data.Length;i++)line.Points.Add(new global::Windows.Foundation.Point(10+i*(w-20)/(data.Length-1),h-12-data[i]/max*(h-45)));canvas.Children.Add(line);}
}
