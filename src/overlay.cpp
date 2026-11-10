// Independent GTK3 / X11 / Cairo / Pango HUD, based on the supplied VTC
// architecture.
#include "model.h"
#undef signals
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <pango/pangocairo.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>
namespace {
struct Hud {
  GtkWidget *window = nullptr;
  int socket = -1;
  pid_t owner = 0;
  QString ownerStart, token;
  QJsonObject state, metrics;
  OverlayConfig config;
  QByteArray last;
  gint64 received = 0, lastRaise = 0;
  bool dirty = true, visible = false;
  int width = 360, height = 220;
  bool stale = false;
  Display *hotkeyDisplay = nullptr;
  KeyCode hotkey = 0;
};
QString procStart(pid_t pid) {
  QString s = readText(QString("/proc/%1/stat").arg(pid));
  auto f = s.mid(s.lastIndexOf(')') + 2).split(' ');
  return f.size() > 19 ? f[19] : QString();
}
void label(cairo_t *cr, const QString &s, double x, double y, int size, double r = 0.9,
           double g = 0.9, double b = 0.9, int maxWidth = 0) {
  PangoLayout *l = pango_cairo_create_layout(cr);
  auto *font = pango_font_description_new();
  pango_font_description_set_family(font, "Sans");
  pango_font_description_set_absolute_size(font, size * PANGO_SCALE);
  pango_layout_set_font_description(l, font);
  QByteArray bytes = s.toUtf8();
  pango_layout_set_text(l, bytes.constData(), bytes.size());
  if (maxWidth > 0) {
    pango_layout_set_width(l, maxWidth * PANGO_SCALE);
    pango_layout_set_ellipsize(l, PANGO_ELLIPSIZE_END);
  }
  cairo_move_to(cr, x, y);
  cairo_set_source_rgb(cr, r, g, b);
  pango_cairo_show_layout(cr, l);
  pango_font_description_free(font);
  g_object_unref(l);
}
gboolean draw(GtkWidget *, cairo_t *cr, gpointer data) {
  auto &h = *static_cast<Hud *>(data);
  auto c = h.config;
  int f = c.font, w = h.width, hh = h.height;
  cairo_set_operator(cr, CAIRO_OPERATOR_SOURCE);
  cairo_set_source_rgba(cr, 0, 0, 0, 0);
  cairo_paint(cr);
  cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
  double radius = c.layout == "compact" ? 4 : 12;
  double pi = 3.141592653589793;
  cairo_new_sub_path(cr);
  cairo_arc(cr, w - radius, radius, radius, -pi / 2, 0);
  cairo_arc(cr, w - radius, hh - radius, radius, 0, pi / 2);
  cairo_arc(cr, radius, hh - radius, radius, pi / 2, pi);
  cairo_arc(cr, radius, radius, radius, pi, 1.5 * pi);
  cairo_close_path(cr);
  cairo_set_source_rgba(cr, .035, .035, .04, c.opacity / 100.);
  cairo_fill(cr);
  GdkRGBA accent{};
  gdk_rgba_parse(&accent, h.metrics["accent"].toString("#76b900").toLatin1().constData());
  cairo_set_source_rgba(cr, accent.red, accent.green, accent.blue, 1);
  cairo_rectangle(cr, 0, 10, 3, hh - 20);
  cairo_fill(cr);
  label(cr, h.metrics["name"].toString("Aguardando telemetria"), 14, 10, f, accent.red,
        accent.green, accent.blue, w - 28);
  QString sub = h.stale                              ? "Telemetria pausada · último quadro"
                : h.metrics["processAlive"].toBool() ? h.metrics["processName"].toString()
                                                     : "Sistema · GPU selecionada";
  label(cr, sub, 14, 14 + f, qMax(9, f - 2), .65, .65, .65, w - 28);
  int i = 0, row = f + 12, top = 2 * f + 28, columns = overlayColumns(c, w);
  for (const auto &key : c.fields) {
    QString name;
    for (const auto &m : metricDefinitions())
      if (m.key == key)
        name = m.label;
    int x = 14 + (i % columns) * (w - 24) / columns, y = top + (i / columns) * row;
    QString value = displayMetric(h.metrics, key);
    label(cr, name + ": " + value, x, y, f, .92, .92, .92, (w - 28) / columns);
    ++i;
  }
  return FALSE;
}
void reposition(Hud &h) {
  auto c = h.config;
  auto *display = gdk_display_get_default();
  int count = gdk_display_get_n_monitors(display);
  auto *monitor = gdk_display_get_monitor(display, qBound(0, c.monitor, qMax(0, count - 1)));
  if (!monitor)
    return;
  GdkRectangle r;
  gdk_monitor_get_geometry(monitor, &r);
  auto rect = overlayRect(c, {r.x, r.y, r.width, r.height});
  h.width = rect.width();
  h.height = rect.height();
  gtk_widget_set_size_request(h.window, h.width, h.height);
  gtk_window_resize(GTK_WINDOW(h.window), h.width, h.height);
  gtk_window_move(GTK_WINDOW(h.window), rect.x(), rect.y());
}
gboolean tick(gpointer data) {
  auto &h = *static_cast<Hud *>(data);
  if (kill(h.owner, 0) != 0 && errno == ESRCH) {
    gtk_main_quit();
    return G_SOURCE_REMOVE;
  }
  if (procStart(h.owner) != h.ownerStart) {
    gtk_main_quit();
    return G_SOURCE_REMOVE;
  }
  if (h.hotkeyDisplay)
    while (XPending(h.hotkeyDisplay)) {
      XEvent e;
      XNextEvent(h.hotkeyDisplay, &e);
      if (e.type == KeyPress && e.xkey.keycode == h.hotkey) {
        printf("TOGGLE\n");
        fflush(stdout);
      }
    }
  char buffer[32768];
  sockaddr_in sender{};
  socklen_t len = sizeof(sender);
  int n;
  while ((n = recvfrom(h.socket, buffer, sizeof buffer, MSG_DONTWAIT,
                       reinterpret_cast<sockaddr *>(&sender), &len)) > 0) {
    if (sender.sin_addr.s_addr != htonl(INADDR_LOOPBACK))
      continue;
    auto doc = QJsonDocument::fromJson(QByteArray(buffer, n));
    auto o = doc.object();
    if (o["version"].toInt() != 1 || o["token"].toString() != h.token)
      continue;
    QByteArray b(buffer, n);
    h.received = g_get_monotonic_time();
    if (b != h.last) {
      h.last = b;
      h.metrics = o["metrics"].toObject();
      auto cfg = o["config"].toObject();
      bool changed = cfg != h.config.json();
      h.config = OverlayConfig::fromJson(cfg);
      if (changed)
        reposition(h);
      h.dirty = true;
    }
  }
  bool stale = h.received && g_get_monotonic_time() - h.received > 3000000;
  if (stale != h.stale) {
    h.stale = stale;
    h.dirty = true;
  }
  bool show =
      h.received && h.config.enabled && (!h.config.onlyGame || h.metrics["processAlive"].toBool());
  if (show != h.visible) {
    h.visible = show;
    if (show) {
      reposition(h);
      gtk_widget_show_all(h.window);
      h.dirty = true;
    } else
      gtk_widget_hide(h.window);
  }
  if (show && h.dirty) {
    gtk_widget_queue_draw(h.window);
    h.dirty = false;
  }
  if (show && g_get_monotonic_time() - h.lastRaise >= 2000000) {
    auto *w = gtk_widget_get_window(h.window);
    if (w)
      gdk_window_raise(w);
    h.lastRaise = g_get_monotonic_time();
  }
  return G_SOURCE_CONTINUE;
}
} // namespace
int overlayChild(int argc, char **argv) {
  QCoreApplication app(argc, argv);
  auto args = app.arguments();
  int ownerIndex = args.indexOf("--owner"), tokenIndex = args.indexOf("--token");
  if (ownerIndex < 0 || ownerIndex + 1 >= args.size() || tokenIndex < 0 ||
      tokenIndex + 1 >= args.size())
    return 2;
  Hud h;
  h.owner = args[ownerIndex + 1].toInt();
  h.ownerStart = procStart(h.owner);
  h.token = args[tokenIndex + 1];
  if (h.owner <= 1 || h.ownerStart.isEmpty() || h.token.size() != 64)
    return 2;
  if (qEnvironmentVariableIsSet("DISPLAY"))
    qputenv("GDK_BACKEND", "x11");
  if (!gtk_init_check(nullptr, nullptr)) {
    fprintf(stderr, "HUD: X11/XWayland indisponível\n");
    return 3;
  }
  if (!GDK_IS_X11_DISPLAY(gdk_display_get_default())) {
    fprintf(stderr, "HUD: este método requer X11/XWayland\n");
    return 3;
  }
  h.socket = socket(AF_INET, SOCK_DGRAM | SOCK_CLOEXEC, 0);
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = 0;
  if (h.socket < 0 || bind(h.socket, reinterpret_cast<sockaddr *>(&addr), sizeof addr) < 0)
    return 4;
  socklen_t length = sizeof addr;
  getsockname(h.socket, reinterpret_cast<sockaddr *>(&addr), &length);
  h.window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_title(GTK_WINDOW(h.window), "Linux GPU Control HUD");
  gtk_window_set_decorated(GTK_WINDOW(h.window), FALSE);
  gtk_window_set_resizable(GTK_WINDOW(h.window), FALSE);
  gtk_window_set_keep_above(GTK_WINDOW(h.window), TRUE);
  gtk_window_set_skip_taskbar_hint(GTK_WINDOW(h.window), TRUE);
  gtk_window_set_skip_pager_hint(GTK_WINDOW(h.window), TRUE);
  gtk_window_set_accept_focus(GTK_WINDOW(h.window), FALSE);
  gtk_window_set_focus_on_map(GTK_WINDOW(h.window), FALSE);
  gtk_widget_set_app_paintable(h.window, TRUE);
  auto *visual = gdk_screen_get_rgba_visual(gtk_widget_get_screen(h.window));
  if (!visual) {
    fprintf(stderr, "HUD: visual RGBA indisponível\n");
    close(h.socket);
    return 5;
  }
  gtk_widget_set_visual(h.window, visual);
  gtk_widget_realize(h.window);
  auto *native = gtk_widget_get_window(h.window);
  gdk_window_set_override_redirect(native, TRUE);
  auto *empty = cairo_region_create();
  gtk_widget_input_shape_combine_region(h.window, empty);
  cairo_region_destroy(empty);
  g_signal_connect(h.window, "draw", G_CALLBACK(draw), &h);
  g_timeout_add(100, tick, &h);
  h.hotkeyDisplay = XOpenDisplay(nullptr);
  if (h.hotkeyDisplay) {
    h.hotkey = XKeysymToKeycode(h.hotkeyDisplay, XK_q);
    auto old = XSetErrorHandler([](Display *, XErrorEvent *) {
      fprintf(stderr, "HUD: Alt+Q indisponível ou reservado por outro aplicativo\n");
      return 0;
    });
    for (unsigned mask :
         {0U, unsigned(LockMask), unsigned(Mod2Mask), unsigned(LockMask | Mod2Mask)})
      XGrabKey(h.hotkeyDisplay, h.hotkey, Mod1Mask | mask, DefaultRootWindow(h.hotkeyDisplay),
               False, GrabModeAsync, GrabModeAsync);
    XSync(h.hotkeyDisplay, False);
    XSetErrorHandler(old);
  }
  printf("READY %u\n", ntohs(addr.sin_port));
  fflush(stdout);
  gtk_main();
  if (h.hotkeyDisplay)
    XCloseDisplay(h.hotkeyDisplay);
  close(h.socket);
  gtk_widget_destroy(h.window);
  return 0;
}
bool renderOverlayPreview(const QString &path, const QString &layout) {
  Hud h;
  h.config = OverlayConfig::fromJson({{"layout", layout},
                                      {"width", layout == "compact"   ? 1100
                                                : layout == "columns" ? 650
                                                                      : 380},
                                      {"font", 16},
                                      {"opacity", 88}});
  h.metrics = {{"name", "NVIDIA · PRÉVIA FICTÍCIA"},
               {"accent", "#76b900"},
               {"usage", 42},
               {"temp", 58},
               {"vram", 4294967296.},
               {"power", 125.4},
               {"clock", 1800}};
  auto rect = overlayRect(h.config, {0, 0, 1920, 1080});
  h.width = rect.width();
  h.height = rect.height();
  auto *surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, h.width, h.height);
  auto *cr = cairo_create(surface);
  draw(nullptr, cr, &h);
  cairo_destroy(cr);
  auto result = cairo_surface_write_to_png(surface, QFile::encodeName(path).constData());
  cairo_surface_destroy(surface);
  return result == CAIRO_STATUS_SUCCESS;
}
