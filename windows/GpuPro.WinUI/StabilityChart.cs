using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Media;
using Microsoft.UI.Xaml.Shapes;
using System.Globalization;
using System.Text;
using System.Text.Json;
namespace GpuPro.Windows;
// Gráfico de estabilidade da aba Overclock (mesma lógica da versão Linux, src/stability_chart.cpp):
// clock, memória, temperatura, potência, ventoinha e uso nos últimos 10 minutos, marcas dos
// ajustes aplicados e diagnóstico sob carga (variação do clock, quedas e causa provável).
internal sealed class StabilityData {
 internal record Sample(DateTime Time,double? Clock,double? Mem,double? Temp,double? Power,double? Fan,double? Usage,double? PowerLimit);
 internal record Verdict(string State,string Detail,int LoadSamples,int Drops,double MeanClock,double Cv,double MaxTemp,double MaxPower);
 internal readonly List<Sample> Samples=new();internal readonly List<(DateTime Time,string Label)> Marks=new();
 internal static readonly TimeSpan Window=TimeSpan.FromMinutes(10);
 internal void Add(JsonElement gpu,double? powerLimitW){
  double? N(string k)=>Native.Number(gpu,k);var now=DateTime.Now;
  Samples.Add(new(now,N("clock"),N("memClock"),N("temp"),N("power"),N("fan"),N("usage"),powerLimitW));
  Samples.RemoveAll(s=>now-s.Time>Window);Marks.RemoveAll(m=>now-m.Time>Window);
 }
 internal void Mark(string label)=>Marks.Add((DateTime.Now,label));
 internal void Clear(){Samples.Clear();Marks.Clear();}
 internal Verdict Evaluate(){
  double maxTemp=Samples.Max(s=>s.Temp)??0,maxPower=Samples.Max(s=>s.Power)??0;
  var clocks=Samples.Where(s=>s.Usage>=80&&s.Clock.HasValue).Select(s=>s.Clock!.Value).ToList();
  if(clocks.Count<10)return new("Sem carga suficiente","Rode um jogo ou o teste de Cenas 3D por pelo menos 10 s para avaliar a estabilidade.",clocks.Count,0,0,0,maxTemp,maxPower);
  double mean=clocks.Average(),cv=mean>0?100*Math.Sqrt(clocks.Sum(c=>(c-mean)*(c-mean))/clocks.Count)/mean:0;int drops=0,thermal=0,power=0;
  foreach(var s in Samples){if(!(s.Usage>=80)||!s.Clock.HasValue||s.Clock>=mean*.93)continue;drops++;if(s.Temp>=83)thermal++;if(s.Power.HasValue&&s.PowerLimit.HasValue&&s.Power>=s.PowerLimit*.97)power++;}
  string cause=thermal>power?"temperatura":power>0?"limite de potência":"";
  if(cv<1.5&&drops==0)return new("Estável",$"Clock sob carga constante: média {mean:0} MHz, variação {cv:0.0} %.",clocks.Count,drops,mean,cv,maxTemp,maxPower);
  if(cv<4&&drops<=3)return new("Oscilação leve",$"Variação de {cv:0.0} % e {drops} queda(s) de clock"+(cause.Length>0?$" · causa provável: {cause}.":"."),clocks.Count,drops,mean,cv,maxTemp,maxPower);
  return new("Instável ou limitado",$"Variação de {cv:0.0} % e {drops} queda(s) de clock sob carga"+(cause.Length>0?$" · causa provável: {cause}. Reduza o ajuste ou melhore a refrigeração.":". Reduza o ajuste ou restaure o padrão."),clocks.Count,drops,mean,cv,maxTemp,maxPower);
 }
 internal string Csv(){
  var b=new StringBuilder("timestamp,clock_mhz,mem_mhz,temp_c,power_w,power_limit_w,fan_pct,usage_pct\n");string F(double? v)=>v.HasValue?v.Value.ToString("0.0",CultureInfo.InvariantCulture):"";
  foreach(var s in Samples)b.Append($"{s.Time:O},{F(s.Clock)},{F(s.Mem)},{F(s.Temp)},{F(s.Power)},{F(s.PowerLimit)},{F(s.Fan)},{F(s.Usage)}\n");
  foreach(var m in Marks)b.Append($"# {m.Time:O} {m.Label}\n");return b.ToString();
 }
}
internal sealed class StabilityChart:Grid {
 internal record Series(string Key,string Label,string Unit,global::Windows.UI.Color Color){internal bool Visible=true;}
 internal readonly List<Series> Lines=new(){new("clock","Clock GPU","MHz",C(118,185,0)),new("mem","Clock memória","MHz",C(63,167,255)),new("temp","Temperatura","°C",C(255,90,95)),new("power","Potência","W",C(255,176,32)),new("fan","Ventoinha","%",C(176,124,255)),new("usage","Uso GPU","%",C(216,216,216))};
 static global::Windows.UI.Color C(byte r,byte g,byte b)=>global::Windows.UI.Color.FromArgb(255,r,g,b);
 readonly Canvas canvas=new();readonly VariableSizedWrapGrid legend=new(){Orientation=Orientation.Horizontal,ItemWidth=200,ItemHeight=20,Margin=new Thickness(46,8,12,0)};readonly StabilityData data;
 internal StabilityChart(StabilityData data){this.data=data;Height=250;Background=new SolidColorBrush(global::Windows.UI.Color.FromArgb(220,17,17,21));CornerRadius=new CornerRadius(8);RowDefinitions.Add(new(){Height=GridLength.Auto});RowDefinitions.Add(new(){Height=new GridLength(1,GridUnitType.Star)});Children.Add(legend);SetRow(canvas,1);Children.Add(canvas);canvas.SizeChanged+=(s,e)=>Draw();}
 static double? Value(StabilityData.Sample s,string key)=>key switch{"clock"=>s.Clock,"mem"=>s.Mem,"temp"=>s.Temp,"power"=>s.Power,"fan"=>s.Fan,_=>s.Usage};
 (double Lo,double Hi) Range(string key){if(key=="temp")return(20,100);if(key is "fan" or "usage")return(0,100);double top=data.Samples.Select(s=>Value(s,key)??0).DefaultIfEmpty(0).Max();if(key=="power")top=Math.Max(top,data.Samples.Select(s=>s.PowerLimit??0).DefaultIfEmpty(0).Max());return(0,top>0?top*1.1:1);}
 static TextBlock Label(string text,global::Windows.UI.Color color,double size=12)=>new(){Text=text,FontSize=size,Foreground=new SolidColorBrush(color)};
 internal void Draw(){
  canvas.Children.Clear();legend.Children.Clear();foreach(var l in Lines.Where(l=>l.Visible)){var current=data.Samples.Count>0&&Value(data.Samples[^1],l.Key) is double v?$"{v:0} {l.Unit}":"N/D";legend.Children.Add(Label("■ "+l.Label+" "+current,l.Color));}
  double w=canvas.ActualWidth,h=canvas.ActualHeight;if(w<120||h<80)return;
  // Legenda com quebra de linha.
  double left=46,top=12,right=w-12,bottom=h-28,height=bottom-top,width=right-left;
  var grid=global::Windows.UI.Color.FromArgb(255,55,55,62);var muted=global::Windows.UI.Color.FromArgb(255,150,150,158);
  for(int i=0;i<=4;i++){double y=top+height*i/4;canvas.Children.Add(new Line{X1=left,X2=right,Y1=y,Y2=y,Stroke=new SolidColorBrush(grid),StrokeThickness=1});var t=Label(100-25*i+"%",muted,11);Canvas.SetLeft(t,4);Canvas.SetTop(t,y-8);canvas.Children.Add(t);}
  var now=data.Samples.Count>0?data.Samples[^1].Time:DateTime.Now;double X(DateTime time)=>right-width*(now-time).TotalMilliseconds/StabilityData.Window.TotalMilliseconds;
  for(int m=0;m<=10;m+=2){var t=Label(m==0?"agora":$"-{m} min",muted,11);Canvas.SetLeft(t,X(now.AddMinutes(-m))-18);Canvas.SetTop(t,bottom+6);canvas.Children.Add(t);}
  foreach(var mark in data.Marks){double x=X(mark.Time);if(x<left)continue;canvas.Children.Add(new Line{X1=x,X2=x,Y1=top,Y2=bottom,Stroke=new SolidColorBrush(global::Windows.UI.Color.FromArgb(150,255,255,255)),StrokeThickness=1,StrokeDashArray=new DoubleCollection{4,3}});var text=mark.Label.Length>48?mark.Label[..48]:mark.Label;var t=Label(text,global::Windows.UI.Color.FromArgb(255,230,230,235),11);double tw=text.Length*6.2;Canvas.SetLeft(t,x+4+tw>right?x-4-tw:x+4);Canvas.SetTop(t,top+2);canvas.Children.Add(t);}
  foreach(var l in Lines.Where(l=>l.Visible)){var (lo,hi)=Range(l.Key);var line=new Polyline{Stroke=new SolidColorBrush(l.Color),StrokeThickness=2};foreach(var s in data.Samples){if(Value(s,l.Key) is not double v){if(line.Points.Count>1)canvas.Children.Add(line);line=new Polyline{Stroke=new SolidColorBrush(l.Color),StrokeThickness=2};continue;}line.Points.Add(new global::Windows.Foundation.Point(X(s.Time),bottom-height*Math.Clamp((v-lo)/(hi-lo),0,1)));}if(line.Points.Count>1)canvas.Children.Add(line);}
  if(data.Samples.Count<2){var t=Label("Aguardando leituras dos sensores",global::Windows.UI.Color.FromArgb(255,170,170,178),13);Canvas.SetLeft(t,left+width/2-110);Canvas.SetTop(t,top+height/2-10);canvas.Children.Add(t);}
 }
}
