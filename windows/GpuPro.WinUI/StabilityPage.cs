using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using Microsoft.UI.Xaml.Documents;
using Microsoft.UI.Xaml.Media;
namespace GpuPro.Windows;
// Seção "Estabilidade" da aba Overclock. A coleta roda o tempo todo (a cada leitura de sensores),
// então o histórico já existe ao abrir a aba; ajustes registrados no log viram marcas no gráfico.
public sealed partial class MainWindow {
 readonly StabilityData stabilityData=new();StabilityChart? stabilityChart;TextBlock? stabilityVerdict;
 void CollectStability(){
  if(snapshot.ValueKind!=System.Text.Json.JsonValueKind.Object)return;
  stabilityData.Add(snapshot.GetProperty("gpu"),Native.Number(specs,"powerLimit") is double mw?mw/1000:null);
  if(stabilityChart!=null){stabilityChart.Draw();RefreshStabilityVerdict();}
 }
 void MarkStability(string text){if(text.Contains("aplic",StringComparison.OrdinalIgnoreCase)||text.Contains("restaur",StringComparison.OrdinalIgnoreCase)||text.Contains("confirm",StringComparison.OrdinalIgnoreCase))stabilityData.Mark(text);}
 void RefreshStabilityVerdict(){
  if(stabilityVerdict==null)return;var v=stabilityData.Evaluate();
  var color=v.State=="Estável"?global::Windows.UI.Color.FromArgb(255,118,185,0):v.State=="Oscilação leve"?global::Windows.UI.Color.FromArgb(255,255,176,32):v.State=="Sem carga suficiente"?global::Windows.UI.Color.FromArgb(255,160,160,168):global::Windows.UI.Color.FromArgb(255,255,90,95);
  stabilityVerdict.Inlines.Clear();stabilityVerdict.Inlines.Add(new Run{Text=v.State,FontWeight=global::Microsoft.UI.Text.FontWeights.SemiBold,Foreground=new SolidColorBrush(color)});
  stabilityVerdict.Inlines.Add(new Run{Text=" — "+v.Detail+(v.LoadSamples>=10?$" · {v.LoadSamples} amostras sob carga · temperatura máx. {v.MaxTemp:0} °C · potência máx. {v.MaxPower:0} W":"")});
 }
 void BuildStabilitySection(){
  stabilityChart=new StabilityChart(stabilityData);stabilityVerdict=new TextBlock{TextWrapping=TextWrapping.Wrap};
  var toggles=new StackPanel{Orientation=Orientation.Horizontal,Spacing=14};
  foreach(var series in stabilityChart.Lines){var check=new CheckBox{Content=series.Label,IsChecked=series.Visible,Foreground=new SolidColorBrush(series.Color),MinWidth=0};check.Click+=(s,e)=>{series.Visible=check.IsChecked==true;stabilityChart.Draw();};toggles.Children.Add(check);}
  var actions=new StackPanel{Orientation=Orientation.Horizontal,Spacing=12};
  actions.Children.Add(Button("Limpar histórico",()=>{stabilityData.Clear();stabilityChart.Draw();RefreshStabilityVerdict();Record("Histórico de estabilidade limpo.");}));
  actions.Children.Add(Button("Exportar CSV",async()=>{try{var picker=new global::Windows.Storage.Pickers.FileSavePicker{SuggestedFileName="GPU-PRO-estabilidade"};WinRT.Interop.InitializeWithWindow.Initialize(picker,hwnd);picker.FileTypeChoices.Add("CSV",new List<string>{".csv"});var f=await picker.PickSaveFileAsync();if(f!=null){File.WriteAllText(f.Path,stabilityData.Csv());Record("Estabilidade exportada: "+f.Path);}}catch(Exception e){Record("Falha ao exportar estabilidade: "+e.Message,true);}}));
  body.Children.Add(Tighten(Section("Estabilidade · últimos 10 minutos",toggles,stabilityChart,stabilityVerdict,actions,
   Text("Cada linha usa a própria escala (0–100 % do gráfico). Linhas tracejadas marcam ajustes aplicados ou restaurados. A avaliação considera apenas momentos com uso da GPU ≥ 80 %: variação do clock, quedas abaixo de 93 % da média e a causa provável (temperatura ≥ 83 °C ou potência ≥ 97 % do limite).",11))));
  RefreshStabilityVerdict();
 }
}
