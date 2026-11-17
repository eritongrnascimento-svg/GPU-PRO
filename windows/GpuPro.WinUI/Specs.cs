using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
using System.Text.Json;
using System.Text.Json.Nodes;
namespace GpuPro.Windows;
// Ficha técnica no mesmo formato da versão Linux: um card por característica, valores
// ausentes ficam N/D e nada é deduzido a partir do nome do modelo.
public sealed partial class MainWindow {
 JsonElement specs;JsonArray? videoCodecs;FitPanel? specGrid;bool specsLoading;
 readonly Dictionary<string,(NeonCard Card,TextBlock Value)> specCards=new();readonly List<string> specOrder=new();
 const string NotReported="Não informado pelo driver";
 static string SpecNumber(JsonElement o,string key,string unit="",double scale=1){var n=Native.Number(o,key);return n.HasValue?(n.Value/scale).ToString(scale==1?"0":"0.0")+(unit.Length>0?" "+unit:""):NotReported;}
 static string SpecText(JsonElement o,string key,string fallback=NotReported){var t=o.ValueKind==JsonValueKind.Object&&o.TryGetProperty(key,out var v)&&v.ValueKind==JsonValueKind.String?v.GetString():null;return string.IsNullOrWhiteSpace(t)?fallback:t;}
 static bool SpecFlag(JsonElement o,string key)=>o.ValueKind==JsonValueKind.Object&&o.TryGetProperty(key,out var v)&&v.ValueKind==JsonValueKind.True;
 UIElement SpecsPanel(){
  var panel=new StackPanel{Spacing=14};
  panel.Children.Add(Button("Atualizar ficha e consultar Vulkan",async()=>await LoadSpecs()));
  specGrid=Fit(1,58);specGrid.MinCardWidth=190;specGrid.MinCardHeight=58;specGrid.MaxCardHeight=120;specGrid.Gap=8;panel.Children.Add(specGrid);
  panel.Children.Add(Text("Clocks e sensores vêm do driver; características não expostas não são adivinhadas. FLOPS e largura de banda medidos pertencem aos benchmarks computacionais, não à ficha técnica. Dados brutos ficam em Feedback & Diagnóstico.",12));
  specCards.Clear();specOrder.Clear();RenderSpecs();if(specs.ValueKind!=JsonValueKind.Object)_=LoadSpecs();return panel;
 }
 async Task LoadSpecs(){
  if(specsLoading)return;specsLoading=true;
  try{specs=await Task.Run(Native.Specs);Record("Ficha técnica atualizada: "+SpecText(specs,"name","GPU"));
   try{var response=await Data(new(){["operation"]="recent",["device"]=DeviceKey,["kind"]="video"});var records=response["records"]?.AsArray();videoCodecs=records is {Count:>0}?records[0]?["codecResults"]?.AsArray():null;}catch(Exception e){videoCodecs=null;Record("Histórico de vídeo indisponível para a ficha: "+e.Message);}
   RenderSpecs();}
  catch(Exception e){Record("Falha ao consultar a ficha técnica: "+e.Message,true);}finally{specsLoading=false;}
 }
 List<(string Label,string Value,string Metric,double? Reading)> SpecRows(){
  var rows=new List<(string,string,string,double?)>();if(specs.ValueKind!=JsonValueKind.Object)return rows;
  var s=specs;var live=snapshot.ValueKind==JsonValueKind.Object?snapshot.GetProperty("gpu"):default;
  bool integrated=SpecFlag(s,"integrated");string vendor=SpecText(s,"vendor","");
  void Add(string label,string value,string metric="static",double? reading=0){rows.Add((label,value,metric,value.StartsWith("Não informado")||value.StartsWith("Não exposto")||value.StartsWith("Não exposta")?null:reading));}
  void Live(string label,string key,string unit,string metric){var n=Native.Number(live,key);Add(label,n.HasValue?n.Value.ToString("0")+" "+unit:NotReported,metric,n);}
  Add("Modelo",SpecText(s,"nvmlName",SpecText(s,"name")));
  Add("Fabricante",vendor.Length>0?vendor:NotReported);
  Add("Arquitetura",SpecText(s,"architecture"));
  Add("Unidades de processamento",SpecNumber(s,"gpuCores",vendor=="NVIDIA"?"CUDA cores":"unidades"));
  var dedicated=Native.Number(live,"vramTotal")??Native.Number(s,"memoryBytes")??Native.Number(s,"dedicatedMemoryBytes");
  Add("Memória dedicada",integrated?"Não se aplica · RAM compartilhada":dedicated.HasValue?(dedicated.Value/1073741824).ToString("0.0")+" GiB":NotReported);
  Add("Memória compartilhada · limite do Windows",SpecNumber(s,"sharedMemoryBytes","GiB",1073741824));
  Add("Barramento de memória",integrated?"Compartilhado com a memória do sistema":SpecNumber(s,"memoryBusWidthBits","bits"));
  Add("Tipo de memória",integrated?"RAM compartilhada (UMA)":"Não informado pelo driver; sem inferência pelo nome do modelo");
  Live("Clock GPU atual","clock","MHz","clock");
  if(integrated)Add("Clock memória atual","Não se aplica como clock de VRAM dedicada");else Live("Clock memória atual","memClock","MHz","memClock");
  Add("Clock máximo GPU reportado",SpecNumber(s,"maxGraphicsMHz","MHz"));
  Add("Clock máximo memória reportado",integrated?"Não se aplica como clock de VRAM dedicada":SpecNumber(s,"maxMemoryMHz","MHz"));
  Live("Potência atual · somente leitura","power","W","power");
  var limit=SpecNumber(s,"powerLimit","W",1000);if(Native.Number(s,"powerMin") is double lo&&Native.Number(s,"powerMax") is double hi)limit+=$" · faixa {lo/1000:0}–{hi/1000:0} W";Add("Limite de potência · leitura",limit);
  Live("Temperatura GPU","temp","°C","temp");
  var hotspot=Native.Number(live,"hotspot");Add("Hotspot",hotspot.HasValue?hotspot.Value.ToString("0")+" °C":"Não exposto · "+SpecText(live,"hotspotStatus","sensor indisponível"),"hotspot",hotspot);
  var memoryTemp=Native.Number(live,"memoryTemp");Add("Temperatura da memória",memoryTemp.HasValue?memoryTemp.Value.ToString("0")+" °C":"Não exposto · "+SpecText(live,"memoryTempStatus","sensor indisponível"),"memoryTemp",memoryTemp);
  if(s.TryGetProperty("fans",out var fans)&&fans.ValueKind==JsonValueKind.Array)foreach(var fan in fans.EnumerateArray()){int index=(int)(Native.Number(fan,"index")??0);var percent=index==0?Native.Number(live,"fan")??Native.Number(fan,"percent"):Native.Number(fan,"percent");Add($"Ventoinha {index+1} · {SpecText(fan,"source","driver")}",percent.HasValue?percent.Value.ToString("0")+" % · "+SpecText(fan,"rpmMeaning",""):NotReported,"fan",percent);}
  string Pcie(string gen,string width){var g=Native.Number(s,gen);var w=Native.Number(s,width);return g.HasValue&&w.HasValue?$"PCIe Gen {g:0} · x{w:0}":NotReported;}
  Add("Interface PCIe atual",integrated?"Não se aplica · GPU integrada":Pcie("pcieGeneration","pcieWidth"));
  Add("Interface PCIe máxima",integrated?"Não se aplica · GPU integrada":Pcie("pcieMaxGeneration","pcieMaxWidth"));
  var vendorDriver=SpecText(s,"nvidiaDriverVersion","");var windowsDriver=SpecText(s,"driverVersion","");
  Add("Driver",vendorDriver.Length+windowsDriver.Length==0?NotReported:string.Join(" · ",new[]{vendorDriver.Length>0?vendor+" "+vendorDriver:"",windowsDriver.Length>0?"Windows "+windowsDriver:""}.Where(x=>x.Length>0)));
  Add("VBIOS",SpecText(s,"vbios"));
  Add("Identificador PCI",SpecText(s,"pciBusId","")is {Length:>0} bus?bus+" · "+SpecText(s,"deviceId",""):SpecText(s,"pciHardwareId"));
  if(videoCodecs==null||videoCodecs.Count==0)Add("Vídeo · H.264 / HEVC / AV1","Ainda não testado; execute Encoder + Decoder para validar cada caminho por hardware",reading:null);
  else foreach(var row in videoCodecs){var c=row?.AsObject();if(c==null)continue;Add((c["codec"]?.ToString()??"codec").ToUpperInvariant()+" · "+c["mode"],c["status"]?.ToString()??NotReported);}
  if(s.TryGetProperty("vulkanApi",out _)){
   Add("Vulkan",SpecText(s,"vulkanApi"));Add("Dispositivo Vulkan",SpecText(s,"vulkanDevice"));Add("Driver Vulkan",SpecText(s,"vulkanDriver"));
   if(integrated)Add("Heap compartilhado reportado",SpecNumber(s,"hostVisibleHeapBytes","GiB · capacidade do driver, não VRAM dedicada",1073741824));
   Add("Ray tracing · extensão",SpecFlag(s,"rayTracingPipeline")?"Exposta pelo driver; depende das features e do jogo":"Não exposta");
   Add("Vulkan Video decode · extensão",SpecFlag(s,"videoDecodeQueue")?"Exposta; codecs específicos dependem das capabilities":"Não exposta nesta consulta");
   Add("Vulkan Video encode · extensão",SpecFlag(s,"videoEncodeQueue")?"Exposta; codecs específicos dependem das capabilities":"Não exposta nesta consulta");
  }else Add("Vulkan",SpecText(s,"vulkanStatus","Não exposto pelo driver"));
  return rows;
 }
 void RenderSpecs(){
  if(specGrid==null)return;var rows=SpecRows();
  if(rows.Count==0){if(specGrid.Children.Count==0)specGrid.Children.Add(Text("Consultando a ficha técnica da GPU selecionada…"));return;}
  var labels=rows.Select(r=>r.Label).ToList();
  if(!labels.SequenceEqual(specOrder)){specGrid.Children.Clear();specOrder.Clear();foreach(var label in labels){if(!specCards.ContainsKey(label)){var card=new NeonCard(label,true);card.Value.FontSize=15;card.ValueFontCap=15;card.Value.IsTextSelectionEnabled=true;specCards[label]=(card,card.Value);}specOrder.Add(label);specGrid.Children.Add(specCards[label].Card);}}
  foreach(var (label,value,metric,reading) in rows){var (card,text)=specCards[label];
   var visible=value.StartsWith("Não informado")?"N/D":value.StartsWith("Não exposto")||value.StartsWith("Não exposta")?"Não exposto pelo driver":value.StartsWith("Ainda não testado")?"Não testado":value;
   text.Text=visible;ToolTipService.SetToolTip(card,value);card.Reading(reading,metric,theme);}
 }
}
