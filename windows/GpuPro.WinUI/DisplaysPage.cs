using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;
namespace GpuPro.Windows;
// Monitores & Telas: tudo gerenciado aqui; um único botão abre as configurações do Windows.
public sealed partial class MainWindow {
 static readonly string[] orientations={"Paisagem","Retrato","Paisagem (invertida)","Retrato (invertido)"};
 void BuildDisplaysPage(){
  List<DisplayControl.Display> displays;try{displays=DisplayControl.Read();}catch(Exception e){Record("Monitores: "+e.Message,true);displays=new();}
  int index=0;foreach(var d in displays){index++;var display=d;
   var current=new DisplayControl.Mode(d.Current.PelsWidth,d.Current.PelsHeight,d.Current.DisplayFrequency);bool rotated=d.Current.Orientation%2==1;
   var sizes=d.Modes.Select(m=>(m.Width,m.Height)).Distinct().OrderByDescending(s=>s.Width*s.Height).ThenByDescending(s=>s.Width).Select(s=>new DisplayControl.Mode(s.Width,s.Height,0)).ToList();
   var resolution=new ComboBox{Header="Resolução",ItemsSource=sizes,MinWidth=170};var nativeW=rotated?current.Height:current.Width;var nativeH=rotated?current.Width:current.Height;resolution.SelectedIndex=Math.Max(0,sizes.FindIndex(s=>s.Width==nativeW&&s.Height==nativeH));
   var rate=new ComboBox{Header="Frequência",MinWidth=130};void Rates(){if(resolution.SelectedItem is DisplayControl.Mode r){var hz=d.Modes.Where(m=>m.Width==r.Width&&m.Height==r.Height).Select(m=>m.Hz).Distinct().OrderByDescending(h=>h).ToList();rate.ItemsSource=hz.Select(h=>h+" Hz").ToList();rate.SelectedIndex=Math.Max(0,hz.IndexOf(current.Hz));}}Rates();resolution.SelectionChanged+=(s,e)=>Rates();
   var orientation=new ComboBox{Header="Orientação",ItemsSource=orientations,SelectedIndex=(int)Math.Min(3,d.Current.Orientation),MinWidth=170};
   var apply=Button("Aplicar modo",async()=>{if(resolution.SelectedItem is not DisplayControl.Mode r||rate.SelectedItem is not string hzText)return;var mode=new DisplayControl.Mode(r.Width,r.Height,uint.Parse(hzText.Split(' ')[0]));await ApplyDisplayMode(display,mode,(uint)Math.Max(0,orientation.SelectedIndex));});
   var rows=new List<UIElement>{Row4(resolution,rate,orientation,apply)};
   var extra=new List<UIElement>();
   if(d.Scales.Length>0&&d.ScaleCurrent.HasValue){var scale=new ComboBox{Header="Escala",ItemsSource=d.Scales.Select(v=>v+" %"+(v==d.ScaleRecommended?" (recomendado)":"")).ToList(),SelectedIndex=Array.IndexOf(d.Scales,d.ScaleCurrent.Value),MinWidth=170};scale.SelectionChanged+=(s,e)=>{if(scale.SelectedIndex<0||d.Scales[scale.SelectedIndex]==d.ScaleCurrent)return;try{DisplayControl.SetScale(display,d.Scales[scale.SelectedIndex]);Record($"Monitor {display.Name}: escala {d.Scales[scale.SelectedIndex]} %");}catch(Exception ex){Record("Monitor "+display.Name+": "+ex.Message,true);}};extra.Add(scale);}
   if(d.HdrSupported){var hdr=new ToggleSwitch{Header="HDR",IsOn=d.HdrEnabled,OnContent="Ativado",OffContent="Desativado"};hdr.Toggled+=(s,e)=>{try{DisplayControl.SetHdr(display,hdr.IsOn);Record($"Monitor {display.Name}: HDR {(hdr.IsOn?"ativado":"desativado")}");}catch(Exception ex){Record("Monitor "+display.Name+": "+ex.Message,true);}};extra.Add(hdr);}
   else extra.Add(Labeled("HDR","Não suportado por este monitor/conexão"));
   if(!d.Primary&&displays.Count>1)extra.Add(Button("Tornar principal",()=>{try{DisplayControl.SetPrimary(displays,display);Record("Monitor principal: "+display.Name);BuildPage();}catch(Exception ex){Record(ex.Message,true);}}));
   rows.Add(Row4(extra.ToArray()));
   var picture=new List<UIElement>();
   if(d.Brightness is var (bMin,bCur,bMax)){var b=new Slider{Header=$"Brilho ({bCur})",Minimum=bMin,Maximum=bMax,Value=bCur,Width=220};b.PointerCaptureLost+=(s,e)=>{if(DisplayControl.SetBrightness(display,(uint)b.Value)){b.Header=$"Brilho ({(uint)b.Value})";Record($"Monitor {display.Name}: brilho {(uint)b.Value}");}else Record("Monitor "+display.Name+": brilho não aceito pelo monitor",true);};picture.Add(b);}
   if(d.Contrast is var (cMin,cCur,cMax)){var c=new Slider{Header=$"Contraste ({cCur})",Minimum=cMin,Maximum=cMax,Value=cCur,Width=220};c.PointerCaptureLost+=(s,e)=>{if(DisplayControl.SetContrast(display,(uint)c.Value)){c.Header=$"Contraste ({(uint)c.Value})";Record($"Monitor {display.Name}: contraste {(uint)c.Value}");}else Record("Monitor "+display.Name+": contraste não aceito pelo monitor",true);};picture.Add(c);}
   if(picture.Count==0)picture.Add(Labeled("Brilho e contraste","O monitor não respondeu ao DDC/CI (ative DDC/CI no menu do monitor)"));
   if(d.HasPath){var profiles=DisplayControl.ColorProfiles().ToList();var profile=new ComboBox{Header="Perfil de cor",ItemsSource=profiles,MinWidth=260,SelectedItem=profiles.FirstOrDefault(p=>string.Equals(p,Path.GetFileName(d.ColorProfile??""),StringComparison.OrdinalIgnoreCase))};
    profile.SelectionChanged+=(s,e)=>{if(profile.SelectedItem is string p&&!string.Equals(p,Path.GetFileName(d.ColorProfile??""),StringComparison.OrdinalIgnoreCase)){try{DisplayControl.SetColorProfile(display,p);d.ColorProfile=p;Record($"Monitor {display.Name}: perfil de cor {p}");}catch(Exception ex){Record(ex.Message,true);}}};picture.Add(profile);}
   rows.Add(Row4(picture.ToArray()));
   var title=$"Monitor {index} · {d.Name}"+(d.Primary?" · principal":"")+$" · {current.Width}×{current.Height} @ {current.Hz} Hz · {d.Adapter}";
   body.Children.Add(Tighten(Section(title,rows.ToArray())));
  }
  if(displays.Count==0)body.Children.Add(Text("Nenhum monitor ativo encontrado."));
  body.Children.Add(Button("Abrir configurações de tela do Windows",()=>Open("ms-settings:display")));
 }
 static StackPanel Row4(params UIElement[] items){var row=new StackPanel{Orientation=Orientation.Horizontal,Spacing=14};foreach(var i in items){if(i is FrameworkElement f)f.VerticalAlignment=VerticalAlignment.Bottom;row.Children.Add(i);}return row;}
 UIElement Labeled(string header,string text){var p=new StackPanel{Spacing=4};p.Children.Add(new TextBlock{Text=header});p.Children.Add(Text(text,12));return p;}
 // Como no Windows: aplica, pergunta se deve manter e volta ao modo anterior em 15 s sem resposta.
 async Task ApplyDisplayMode(DisplayControl.Display d,DisplayControl.Mode mode,uint orientation){
  try{DisplayControl.ApplyMode(d,mode,orientation);}catch(Exception e){Record("Monitor "+d.Name+": "+e.Message,true);return;}
  var seconds=15;var text=Text($"Manter {mode.Width} × {mode.Height} @ {mode.Hz} Hz · {orientations[orientation]}? Volta ao modo anterior em {seconds} s.");
  var dialog=new ContentDialog{XamlRoot=Root.XamlRoot,Title="Confirmar modo de exibição",Content=text,PrimaryButtonText="Manter",CloseButtonText="Reverter",DefaultButton=ContentDialogButton.Close};
  var timer=new DispatcherTimer{Interval=TimeSpan.FromSeconds(1)};timer.Tick+=(s,e)=>{seconds--;text.Text=$"Manter {mode.Width} × {mode.Height} @ {mode.Hz} Hz · {orientations[orientation]}? Volta ao modo anterior em {seconds} s.";if(seconds<=0){timer.Stop();dialog.Hide();}};timer.Start();
  var answer=await dialog.ShowAsync();timer.Stop();
  if(answer==ContentDialogResult.Primary)Record($"Monitor {d.Name}: {mode.Width}×{mode.Height} @ {mode.Hz} Hz · {orientations[orientation]}");
  else{DisplayControl.Restore(d);Record($"Monitor {d.Name}: modo revertido para {d.Current.PelsWidth}×{d.Current.PelsHeight} @ {d.Current.DisplayFrequency} Hz");}
  BuildPage();
 }
}
