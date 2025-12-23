#import <ApplicationServices/ApplicationServices.h>
#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>
#include <math.h>

#define WORK_DURATION (25 * 60)
#define BREAK_DURATION (5 * 60)
#define WINDOW_WIDTH 200
#define WINDOW_HEIGHT 200

typedef enum { STATE_WORK, STATE_BREAK } TimerState;

@interface PomoView : NSView
@property(nonatomic, assign) double secRemaining;
@property(nonatomic, assign) TimerState state;
@property(nonatomic, assign) BOOL paused;
@end

@interface PomoWindow : NSWindow
@end

@interface PomoAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) NSWindow *window;
@property(nonatomic, strong) PomoView *pomoView;
@property(nonatomic, strong) NSTimer *timer;
- (void)handleKeyDown:(NSEvent *)event;
- (void)snapToNearestCorner;
@end

@implementation PomoView

- (BOOL)isFlipped {
  return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
  CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
  CGFloat cx = self.bounds.size.width / 2.0;
  CGFloat cy = self.bounds.size.height / 2.0;
  CGFloat outerR = 80.0;
  CGFloat innerR = 75.0;
  CGFloat thickness = outerR - innerR;
  CGFloat midR = (outerR + innerR) / 2.0;

  //draw background ring
  CGContextSetLineWidth(ctx, thickness);
  CGContextSetLineCap(ctx, kCGLineCapRound);
  [[NSColor colorWithDeviceRed:0.15 green:0.17 blue:0.25 alpha:0.7] setStroke];
  CGContextAddArc(ctx, cx, cy, midR, 0, 2 * M_PI, 0);
  CGContextStrokePath(ctx);

  // Draw progress ring
  double total = (self.state == STATE_WORK) ? WORK_DURATION : BREAK_DURATION;
  double prog = fmax(0.0, self.secRemaining / total);
  if (prog > 0) {
    if (self.state == STATE_WORK) {
      [[NSColor colorWithDeviceRed:1.0 green:0.45 blue:0.45
                             alpha:1.0] setStroke];
    } else {
      [[NSColor colorWithDeviceRed:0.4 green:0.85 blue:1.0
                             alpha:1.0] setStroke];
    }

    // Progress goes clockwise from top
    CGFloat startAngle = -M_PI_2;
    CGFloat endAngle = startAngle + (2 * M_PI * prog);
    CGContextAddArc(ctx, cx, cy, midR, startAngle, endAngle, 0);
    CGContextStrokePath(ctx);
  }

  // Draw text
  int s = (int)ceil(self.secRemaining);
  NSString *timeStr = [NSString stringWithFormat:@"%02d:%02d", s / 60, s % 60];
  NSString *labelStr =
      self.paused
          ? @"PAUSED BAD BOY :|"
          : (self.state == STATE_WORK ? @"GOOD BOY :3" : @"BREAK! ENJOY");

  NSMutableParagraphStyle *style = [[NSMutableParagraphStyle alloc] init];
  style.alignment = NSTextAlignmentCenter;

  NSDictionary *timeAttrs = @{
    NSFontAttributeName : [NSFont fontWithName:@"HelveticaNeue-Medium" size:40],
    NSForegroundColorAttributeName : [NSColor whiteColor],
    NSParagraphStyleAttributeName : style
  };

  NSDictionary *labelAttrs = @{
    NSFontAttributeName : [NSFont fontWithName:@"HelveticaNeue" size:10],
    NSForegroundColorAttributeName : [NSColor colorWithDeviceRed:0.75
                                                           green:0.78
                                                            blue:0.9
                                                           alpha:0.9],
    NSParagraphStyleAttributeName : style
  };

  NSSize timeSize = [timeStr sizeWithAttributes:timeAttrs];
  NSRect timeRect = NSMakeRect(0, cy - timeSize.height / 2.0 - 5,
                               self.bounds.size.width, timeSize.height);

  CGFloat alpha = 1.0;
  if (self.paused) {
    alpha = 0.6 + 0.4 * sin([[NSDate date] timeIntervalSince1970] * 6.0);
  }

  CGContextSetAlpha(ctx, alpha);
  [timeStr drawInRect:timeRect withAttributes:timeAttrs];

  NSSize labelSize = [labelStr sizeWithAttributes:labelAttrs];
  NSRect labelRect =
      NSMakeRect(0, cy + 25, self.bounds.size.width, labelSize.height);
  CGContextSetAlpha(ctx, 0.9);
  [labelStr drawInRect:labelRect withAttributes:labelAttrs];
}

- (void)mouseDown:(NSEvent *)event {
  [self.window performWindowDragWithEvent:event];
  // After performWindowDragWithEvent returns, the drag is finished.
  [(PomoAppDelegate *)[NSApp delegate] snapToNearestCorner];
}

@end

@implementation PomoWindow
- (BOOL)canBecomeKeyWindow {
  return YES;
}
- (void)keyDown:(NSEvent *)event {
  [(PomoAppDelegate *)[NSApp delegate] handleKeyDown:event];
}
@end

@implementation PomoAppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  NSRect frame = NSMakeRect(0, 0, WINDOW_WIDTH, WINDOW_HEIGHT);
  self.window =
      [[PomoWindow alloc] initWithContentRect:frame
                                    styleMask:NSWindowStyleMaskBorderless
                                      backing:NSBackingStoreBuffered
                                        defer:NO];

  [self.window setOpaque:NO];
  [self.window setBackgroundColor:[NSColor clearColor]];
  [self.window setHasShadow:NO];
  [self.window setLevel:NSFloatingWindowLevel];
  [self.window setMovableByWindowBackground:NO];
  [self.window center];

  self.pomoView = [[PomoView alloc] initWithFrame:frame];
  self.pomoView.secRemaining = WORK_DURATION;
  self.pomoView.state = STATE_WORK;
  self.pomoView.paused = NO;

  [self.window setContentView:self.pomoView];
  [self.window makeKeyAndOrderFront:nil];
  [NSApp activateIgnoringOtherApps:YES];

  self.timer = [NSTimer scheduledTimerWithTimeInterval:1.0 / 60.0
                                                target:self
                                              selector:@selector(tick:)
                                              userInfo:nil
                                               repeats:YES];
  [[NSRunLoop mainRunLoop] addTimer:self.timer forMode:NSRunLoopCommonModes];
}

- (void)applicationDidResignActive:(NSNotification *)notification {
  [self snapToNearestCorner];
}

- (void)tick:(NSTimer *)timer {
  if (!self.pomoView.paused) {
    self.pomoView.secRemaining -= 1.0 / 60.0;
    if (self.pomoView.secRemaining <= 0) {
      if (self.pomoView.state == STATE_WORK) {
        self.pomoView.state = STATE_BREAK;
        self.pomoView.secRemaining = BREAK_DURATION;
      } else {
        self.pomoView.state = STATE_WORK;
        self.pomoView.secRemaining = WORK_DURATION;
      }
    }
  }
  [self.pomoView setNeedsDisplay:YES];
}

- (void)snapToNearestCorner {
  NSRect myFrame = self.window.frame;
  NSScreen *screen = [self.window screen];
  if (!screen)
    screen = [NSScreen mainScreen];
  NSRect sFrame = [screen visibleFrame];

  // 4 possible corners
  NSPoint topLeft =
      NSMakePoint(NSMinX(sFrame), NSMaxY(sFrame) - myFrame.size.height);
  NSPoint topRight = NSMakePoint(NSMaxX(sFrame) - myFrame.size.width,
                                 NSMaxY(sFrame) - myFrame.size.height);
  NSPoint bottomLeft = NSMakePoint(NSMinX(sFrame), NSMinY(sFrame));
  NSPoint bottomRight =
      NSMakePoint(NSMaxX(sFrame) - myFrame.size.width, NSMinY(sFrame));

  NSPoint corners[] = {topLeft, topRight, bottomLeft, bottomRight};
  NSPoint nearestCorner = corners[0];
  CGFloat minDist = CGFLOAT_MAX;

  for (int i = 0; i < 4; i++) {
    CGFloat dx = myFrame.origin.x - corners[i].x;
    CGFloat dy = myFrame.origin.y - corners[i].y;
    CGFloat dist = dx * dx + dy * dy;
    if (dist < minDist) {
      minDist = dist;
      nearestCorner = corners[i];
    }
  }

  NSRect snapFrame = myFrame;
  snapFrame.origin = nearestCorner;

  if (!NSEqualRects(myFrame, snapFrame)) {
    [self.window setFrame:snapFrame display:YES animate:YES];
  }
}

- (void)handleKeyDown:(NSEvent *)event {
  if (event.keyCode == 49) { // Space
    self.pomoView.paused = !self.pomoView.paused;
  } else if (event.keyCode == 53) { // Escape
    [NSApp terminate:nil];
  }
}

@end

int main(int argc, const char *argv[]) {
  @autoreleasepool {
    PomoAppDelegate *delegate = [[PomoAppDelegate alloc] init];
    [NSApplication sharedApplication];
    [NSApp setDelegate:delegate];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    [NSApp run];
  }
  return 0;
}
