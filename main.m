#import <AVFoundation/AVFoundation.h>
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
@property(nonatomic, assign) BOOL isAway;
@property(nonatomic, assign) int workMin;
@property(nonatomic, assign) int breakMin;
@end

@interface StreakGraphView : NSView
@property(nonatomic, strong) NSDictionary *history;
@end

@interface PomoWindow : NSWindow
@end

@interface PomoAppDelegate : NSObject <NSApplicationDelegate>
@property(nonatomic, strong) PomoWindow *window;
@property(nonatomic, strong) PomoView *pomoView;
@property(nonatomic, strong) NSTimer *timer;
@property(nonatomic, assign) double pauseDuration;
@property(nonatomic, assign) NSPoint baseOrigin;
@property(nonatomic, assign) BOOL isShaking;
@property(nonatomic, strong) AVAudioPlayer *audioPlayer;
@property(nonatomic, assign) double elapsedWork;
@property(nonatomic, assign) double elapsedBreak;
@property(nonatomic, assign) int workMin;
@property(nonatomic, assign) int breakMin;
@property(nonatomic, assign) int longBreakMin;
@property(nonatomic, assign) int sessionsUntilLong;
@property(nonatomic, assign) BOOL soundOn;
@property(nonatomic, assign) BOOL autoStart;
@property(nonatomic, assign) int opacity;
@property(nonatomic, assign) int volume;
@property(nonatomic, assign) int focusThreshold;
@property(nonatomic, assign) int sessionCount;
@property(nonatomic, assign) BOOL isAway;
@property(nonatomic, strong) NSString *lastDate;
@property(nonatomic, assign) int dailySessions;
@property(nonatomic, assign) int consecutiveDays;
@property(nonatomic, strong) NSMutableDictionary *history;
@property(nonatomic, strong) NSWindow *streakWindow;
@property(nonatomic, strong) NSWindow *settingsWindow;
@property(nonatomic, strong) NSTextField *workField;
@property(nonatomic, strong) NSTextField *breakField;
@property(nonatomic, strong) NSTextField *longBreakField;
@property(nonatomic, strong) NSTextField *sessionsField;
@property(nonatomic, strong) NSButton *soundCheck;
@property(nonatomic, strong) NSButton *autoStartCheck;
@property(nonatomic, strong) NSSlider *opacitySlider;
@property(nonatomic, strong) NSSlider *volumeSlider;
@property(nonatomic, strong) NSTextField *focusField;

- (void)handleKeyDown:(NSEvent *)event;
- (void)snapToNearestCorner;
- (void)loadConfig;
- (void)saveConfig;
- (void)resetTimer;
- (void)showSettings;
- (void)showStreak;
- (void)applySettings;
- (void)loadStreak;
- (void)saveStreak;
- (void)updateStreak;
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

  // Draw background ring
  CGContextSetLineWidth(ctx, thickness);
  CGContextSetLineCap(ctx, kCGLineCapRound);
  [[NSColor colorWithDeviceRed:0.15 green:0.17 blue:0.25 alpha:0.7] setStroke];
  CGContextAddArc(ctx, cx, cy, midR, 0, 2 * M_PI, 0);
  CGContextStrokePath(ctx);

  // Draw progress ring
  double total =
      (self.state == STATE_WORK) ? self.workMin * 60.0 : self.breakMin * 60.0;
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
          ? @"PAUSED"
          : (self.state == STATE_WORK
                 ? [NSString
                       stringWithFormat:@"GOOD BOY :3 Session #%d",
                                        [(PomoAppDelegate *)[NSApp delegate]
                                            sessionCount] +
                                            1]
                 : @"BREAK! ENJOY");

  if (self.isAway) {
    labelStr = @"AWAY? FOCUS!";
  }

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
  [(PomoAppDelegate *)[NSApp delegate] setIsAway:NO];
  PomoAppDelegate *ad = (PomoAppDelegate *)[NSApp delegate];
  if (ad.isShaking) {
    NSRect frame = ad.window.frame;
    frame.origin = ad.baseOrigin;
    [ad.window setFrame:frame display:YES animate:NO];
    ad.isShaking = NO;
    ad.pauseDuration = 0;
  }
  [self.window performWindowDragWithEvent:event];
  // After performWindowDragWithEvent returns, the drag is finished.
  [(PomoAppDelegate *)[NSApp delegate] snapToNearestCorner];
}

@end

@implementation StreakGraphView

- (BOOL)isFlipped {
  return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
  CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];
  [[NSColor colorWithDeviceRed:13 / 255.0
                         green:17 / 255.0
                          blue:23 / 255.0
                         alpha:1.0] set];
  NSRectFill(self.bounds);

  CGFloat startX = 60;
  CGFloat startY = 30;
  CGFloat sqSize = 10;
  CGFloat gap = 3;

  NSColor *gray = [NSColor colorWithDeviceRed:139 / 255.0
                                        green:148 / 255.0
                                         blue:158 / 255.0
                                        alpha:1.0];
  NSDictionary *labelAttrs = @{
    NSFontAttributeName : [NSFont systemFontOfSize:9],
    NSForegroundColorAttributeName : gray
  };

  NSArray *days = @[ @"Sun", @"Mon", @"Tue", @"Wed", @"Thu", @"Fri", @"Sat" ];
  for (int i = 0; i < 7; i++) {
    if (i % 2 == 1) {
      [[days objectAtIndex:i]
             drawAtPoint:NSMakePoint(15, startY + i * (sqSize + gap) - 2)
          withAttributes:labelAttrs];
    }
  }

  NSArray *months = @[
    @"Jan", @"Feb", @"Mar", @"Apr", @"May", @"Jun", @"Jul", @"Aug", @"Sep",
    @"Oct", @"Nov", @"Dec"
  ];
  for (int i = 0; i < 12; i++) {
    [[months objectAtIndex:i]
           drawAtPoint:NSMakePoint(startX + i * 53, startY - 20)
        withAttributes:labelAttrs];
  }

  NSDate *now = [NSDate date];
  NSCalendar *cal = [NSCalendar currentCalendar];
  NSDateComponents *comp = [cal components:NSCalendarUnitWeekday fromDate:now];
  int todayDow = (int)comp.weekday - 1;

  NSDateFormatter *df = [[NSDateFormatter alloc] init];
  [df setDateFormat:@"yyyy-MM-dd"];

  for (int w = 0; w < 52; w++) {
    for (int d = 0; d < 7; d++) {
      int daysAgo = (51 - w) * 7 + (todayDow - d);
      if (daysAgo < 0)
        continue;

      NSDate *cellDate = [now dateByAddingTimeInterval:-daysAgo * 86400];
      NSString *dateStr = [df stringFromDate:cellDate];
      NSNumber *sessNum = [self.history objectForKey:dateStr];
      int sessions = sessNum ? [sessNum intValue] : 0;

      NSColor *color;
      if (sessions == 0)
        color = [NSColor colorWithDeviceRed:22 / 255.0
                                      green:27 / 255.0
                                       blue:34 / 255.0
                                      alpha:1.0];
      else if (sessions < 2)
        color = [NSColor colorWithDeviceRed:14 / 255.0
                                      green:68 / 255.0
                                       blue:41 / 255.0
                                      alpha:1.0];
      else if (sessions < 4)
        color = [NSColor colorWithDeviceRed:0 / 255.0
                                      green:109 / 255.0
                                       blue:50 / 255.0
                                      alpha:1.0];
      else if (sessions < 6)
        color = [NSColor colorWithDeviceRed:38 / 255.0
                                      green:166 / 255.0
                                       blue:65 / 255.0
                                      alpha:1.0];
      else
        color = [NSColor colorWithDeviceRed:57 / 255.0
                                      green:211 / 255.0
                                       blue:83 / 255.0
                                      alpha:1.0];

      [color setFill];
      NSRectFill(NSMakeRect(startX + w * (sqSize + gap),
                            startY + d * (sqSize + gap), sqSize, sqSize));
    }
  }

  CGFloat legX = startX + 52 * (sqSize + gap) - 100;
  CGFloat legY = startY + 7 * (sqSize + gap) + 10;
  [@"Less" drawAtPoint:NSMakePoint(legX - 30, legY - 2)
        withAttributes:labelAttrs];

  for (int i = 0; i < 5; i++) {
    NSColor *lc;
    if (i == 0)
      lc = [NSColor colorWithDeviceRed:22 / 255.0
                                 green:27 / 255.0
                                  blue:34 / 255.0
                                 alpha:1.0];
    else if (i == 1)
      lc = [NSColor colorWithDeviceRed:14 / 255.0
                                 green:68 / 255.0
                                  blue:41 / 255.0
                                 alpha:1.0];
    else if (i == 2)
      lc = [NSColor colorWithDeviceRed:0 / 255.0
                                 green:109 / 255.0
                                  blue:50 / 255.0
                                 alpha:1.0];
    else if (i == 3)
      lc = [NSColor colorWithDeviceRed:38 / 255.0
                                 green:166 / 255.0
                                  blue:65 / 255.0
                                 alpha:1.0];
    else
      lc = [NSColor colorWithDeviceRed:57 / 255.0
                                 green:211 / 255.0
                                  blue:83 / 255.0
                                 alpha:1.0];
    [lc setFill];
    NSRectFill(NSMakeRect(legX + i * (sqSize + gap), legY, sqSize, sqSize));
  }
  [@"More" drawAtPoint:NSMakePoint(legX + 5 * (sqSize + gap) + 5, legY - 2)
        withAttributes:labelAttrs];
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
  [self loadConfig];
  [self loadStreak];
  self.pomoView.workMin = self.workMin;
  self.pomoView.breakMin = self.breakMin;
  self.pomoView.secRemaining = self.workMin * 60.0;
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

  if (self.soundOn) {
    NSURL *audioURL = [[NSBundle mainBundle] URLForResource:@"res/timer"
                                              withExtension:@"mp3"];
    if (!audioURL) {
      NSString *path = [[[[NSProcessInfo processInfo] arguments]
          objectAtIndex:0] stringByDeletingLastPathComponent];
      path = [path stringByAppendingPathComponent:@"res/timer.mp3"];
      audioURL = [NSURL fileURLWithPath:path];
    }

    NSError *error = nil;
    self.audioPlayer = [[AVAudioPlayer alloc] initWithContentsOfURL:audioURL
                                                              error:&error];
    if (self.audioPlayer) {
      self.audioPlayer.numberOfLoops = -1;
      self.audioPlayer.volume = self.volume / 128.0;
      [self.audioPlayer prepareToPlay];
      [self.audioPlayer play];
    } else {
      NSLog(@"Failed to load audio: %@", error);
    }
  }
}

- (void)applicationDidResignActive:(NSNotification *)notification {
  [self snapToNearestCorner];
}

- (void)applicationDidBecomeActive:(NSNotification *)notification {
  int oldW = self.workMin;
  int oldB = self.breakMin;
  BOOL oldS = self.soundOn;
  [self loadConfig];
  self.pomoView.workMin = self.workMin;
  self.pomoView.breakMin = self.breakMin;
  if (oldW != self.workMin || oldB != self.breakMin) {
    [self resetTimer];
  }
  if (oldS != self.soundOn) {
    if (self.soundOn) {
      [self.audioPlayer play];
    } else {
      [self.audioPlayer stop];
    }
  }
}

- (void)tick:(NSTimer *)timer {
  if (!self.pomoView.paused) {
    double idle = CGEventSourceSecondsSinceLastEventType(
        kCGEventSourceStateCombinedSessionState, kCGAnyInputEventType);
    if (idle > self.focusThreshold && self.pomoView.state == STATE_WORK &&
        !self.pomoView.paused) {
      self.pomoView.paused = YES;
      self.isAway = YES;
      self.pomoView.isAway = YES;
      [self.audioPlayer pause];
    }

    if (self.isShaking) {
      NSRect frame = self.window.frame;
      frame.origin = self.baseOrigin;
      [self.window setFrame:frame display:YES animate:NO];
      self.isShaking = NO;
    }
    self.pauseDuration = 0;
    if (self.audioPlayer && !self.audioPlayer.playing && self.soundOn) {
      [self.audioPlayer play];
    }

    // sync audio
    if (self.audioPlayer && self.soundOn) {
      double targetPos;
      if (self.pomoView.state == STATE_WORK) {
        targetPos = fmod(self.elapsedWork, 1500.0);
      } else {
        targetPos = 1500.0 + fmod(self.elapsedBreak, 300.0);
      }

      if (fabs(self.audioPlayer.currentTime - targetPos) > 1.0) {
        self.audioPlayer.currentTime = targetPos;
      }
    }

    self.pomoView.secRemaining -= 1.0 / 60.0;
    if (self.pomoView.state == STATE_WORK) {
      self.elapsedWork += 1.0 / 60.0;
    } else {
      self.elapsedBreak += 1.0 / 60.0;
    }

    if (self.pomoView.secRemaining <= 0) {
      if (self.pomoView.state == STATE_WORK) {
        self.sessionCount++;
        [self updateStreak];
        if (self.sessionCount % self.sessionsUntilLong == 0) {
          self.pomoView.state = STATE_BREAK;
          self.pomoView.secRemaining = self.longBreakMin * 60.0;
        } else {
          self.pomoView.state = STATE_BREAK;
          self.pomoView.secRemaining = self.breakMin * 60.0;
        }
      } else {
        self.pomoView.state = STATE_WORK;
        self.pomoView.secRemaining = self.workMin * 60.0;
        self.elapsedWork = 0;
        self.elapsedBreak = 0;
      }
      if (!self.autoStart) {
        self.pomoView.paused = YES;
        [self.audioPlayer pause];
      }
    }
  } else {
    if (self.audioPlayer && self.audioPlayer.playing) {
      [self.audioPlayer pause];
    }
    self.pauseDuration += 1.0 / 60.0;
    if (self.pauseDuration > 300.0 &&
        self.pomoView.state == STATE_WORK) { // 5 minutes
      if (!self.isShaking) {
        self.baseOrigin = self.window.frame.origin;
        self.isShaking = YES;
      }
      NSRect frame = self.window.frame;
      int offsetX = (rand() % 5) - 2;
      int offsetY = (rand() % 5) - 2;
      frame.origin.x = self.baseOrigin.x + offsetX;
      frame.origin.y = self.baseOrigin.y + offsetY;
      [self.window setFrame:frame display:YES animate:NO];
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
    [self saveConfig];
  }
}

- (void)loadConfig {
  self.workMin = 25;
  self.breakMin = 5;
  self.longBreakMin = 15;
  self.sessionsUntilLong = 4;
  self.soundOn = YES;
  self.autoStart = NO;
  self.opacity = 100;
  self.volume = 128;
  self.focusThreshold = 60;

  NSString *path = @"pomo.cfg";
  NSString *content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:nil];
  if (content) {
    NSArray *lines = [content componentsSeparatedByString:@"\n"];
    for (NSString *line in lines) {
      NSArray *parts = [line componentsSeparatedByString:@"="];
      if (parts.count == 2) {
        NSString *key = parts[0];
        int val = [parts[1] intValue];
        if ([key isEqualToString:@"work_time"])
          self.workMin = val;
        else if ([key isEqualToString:@"break_time"])
          self.breakMin = val;
        else if ([key isEqualToString:@"long_break_time"])
          self.longBreakMin = val;
        else if ([key isEqualToString:@"sessions_until_long"])
          self.sessionsUntilLong = val;
        else if ([key isEqualToString:@"sound"])
          self.soundOn = (val != 0);
        else if ([key isEqualToString:@"auto_start"])
          self.autoStart = (val != 0);
        else if ([key isEqualToString:@"opacity"])
          self.opacity = val;
        else if ([key isEqualToString:@"volume"])
          self.volume = val;
        else if ([key isEqualToString:@"focus_threshold"])
          self.focusThreshold = val;
        else if ([key isEqualToString:@"x"]) {
          NSRect frame = self.window.frame;
          frame.origin.x = val;
          [self.window setFrame:frame display:YES];
        } else if ([key isEqualToString:@"y"]) {
          NSRect frame = self.window.frame;
          frame.origin.y = val;
          [self.window setFrame:frame display:YES];
        }
      }
    }
  }
  [self.window setAlphaValue:self.opacity / 100.0];
  if (self.audioPlayer) {
    self.audioPlayer.volume = self.volume / 128.0;
  }
}

- (void)saveConfig {
  NSMutableString *s = [NSMutableString string];
  [s appendFormat:@"work_time=%d\n", self.workMin];
  [s appendFormat:@"break_time=%d\n", self.breakMin];
  [s appendFormat:@"long_break_time=%d\n", self.longBreakMin];
  [s appendFormat:@"sessions_until_long=%d\n", self.sessionsUntilLong];
  [s appendFormat:@"sound=%d\n", self.soundOn ? 1 : 0];
  [s appendFormat:@"auto_start=%d\n", self.autoStart ? 1 : 0];
  [s appendFormat:@"opacity=%d\n", self.opacity];
  [s appendFormat:@"volume=%d\n", self.volume];
  [s appendFormat:@"focus_threshold=%d\n", self.focusThreshold];
  [s appendFormat:@"x=%d\n", (int)self.window.frame.origin.x];
  [s appendFormat:@"y=%d\n", (int)self.window.frame.origin.y];
  [s writeToFile:@"pomo.cfg"
      atomically:YES
        encoding:NSUTF8StringEncoding
           error:nil];
}

- (void)loadStreak {
  self.lastDate = @"0000-00-00";
  self.dailySessions = 0;
  self.consecutiveDays = 0;
  self.history = [NSMutableDictionary dictionary];

  NSString *content = [NSString stringWithContentsOfFile:@"streak.txt"
                                                encoding:NSUTF8StringEncoding
                                                   error:nil];
  if (content) {
    NSArray *lines = [content componentsSeparatedByString:@"\n"];
    for (NSString *line in lines) {
      NSArray *parts = [line componentsSeparatedByString:@"="];
      if (parts.count == 2) {
        if ([parts[0] isEqualToString:@"last_date"])
          self.lastDate = parts[1];
        else if ([parts[0] isEqualToString:@"daily_sessions"])
          self.dailySessions = [parts[1] intValue];
        else if ([parts[0] isEqualToString:@"consecutive_days"])
          self.consecutiveDays = [parts[1] intValue];
        else if ([parts[0] hasPrefix:@"h:"]) {
          NSString *dateKey = [parts[0] substringFromIndex:2];
          [self.history setObject:@([parts[1] intValue]) forKey:dateKey];
        }
      }
    }
  }
}

- (void)saveStreak {
  NSMutableString *s = [NSMutableString string];
  [s appendFormat:@"last_date=%@\n", self.lastDate];
  [s appendFormat:@"daily_sessions=%d\n", self.dailySessions];
  [s appendFormat:@"consecutive_days=%d\n", self.consecutiveDays];
  for (NSString *key in self.history) {
    [s appendFormat:@"h:%@=%@\n", key, self.history[key]];
  }
  [s writeToFile:@"streak.txt"
      atomically:YES
        encoding:NSUTF8StringEncoding
           error:nil];
}

- (void)updateStreak {
  NSDateFormatter *df = [[NSDateFormatter alloc] init];
  [df setDateFormat:@"yyyy-MM-dd"];
  NSString *today = [df stringFromDate:[NSDate date]];

  if ([self.lastDate isEqualToString:today]) {
    self.dailySessions++;
  } else {
    NSDate *lastDateObj = [df dateFromString:self.lastDate];
    if (lastDateObj) {
      NSTimeInterval diff = [[NSDate date] timeIntervalSinceDate:lastDateObj];
      if (diff <= 86400 * 1.5) {
        self.consecutiveDays++;
      } else {
        self.consecutiveDays = 1;
      }
    } else {
      self.consecutiveDays = 1;
    }
    self.dailySessions = 1;
    self.lastDate = today;
  }
  [self.history setObject:@(self.dailySessions) forKey:today];
  [self saveStreak];
}

- (void)resetTimer {
  self.pomoView.state = STATE_WORK;
  self.pomoView.secRemaining = self.workMin * 60.0;
  self.pomoView.paused = NO;
  self.elapsedWork = 0;
  self.elapsedBreak = 0;
  self.pauseDuration = 0;
  if (self.isShaking) {
    NSRect frame = self.window.frame;
    frame.origin = self.baseOrigin;
    [self.window setFrame:frame display:YES animate:NO];
    self.isShaking = NO;
  }
  if (self.audioPlayer && self.soundOn) {
    self.audioPlayer.currentTime = 0;
    [self.audioPlayer play];
  }
}

- (void)handleKeyDown:(NSEvent *)event {
  self.isAway = NO;
  self.pomoView.isAway = NO;
  if (self.isShaking) {
    NSRect frame = self.window.frame;
    frame.origin = self.baseOrigin;
    [self.window setFrame:frame display:YES animate:NO];
    self.isShaking = NO;
    self.pauseDuration = 0;
  }
  if (event.keyCode == 49) { // Space
    self.pomoView.paused = !self.pomoView.paused;
  } else if (event.keyCode == 15) { // 'r'
    [self resetTimer];
  } else if (event.keyCode == 1) { // 's'
    [self showSettings];
  } else if (event.keyCode == 31) { // 'o'
    [self showStreak];
  } else if (event.keyCode == 53) { // Escape
    [NSApp terminate:nil];
  }
}

- (void)showSettings {
  if (self.settingsWindow) {
    [self.settingsWindow makeKeyAndOrderFront:nil];
    return;
  }

  NSRect frame = NSMakeRect(0, 0, 300, 380);
  self.settingsWindow = [[NSWindow alloc]
      initWithContentRect:frame
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  [self.settingsWindow setTitle:@"Advanced Settings"];
  [self.settingsWindow center];

  NSView *cv = [[NSView alloc] initWithFrame:frame];
  [self.settingsWindow setContentView:cv];

  CGFloat y = 310;
  // Work
  [[NSTextField labelWithString:@"Work (min):"]
      setFrame:NSMakeRect(20, y, 120, 20)];
  [cv addSubview:[cv.subviews lastObject]];
  self.workField = [NSTextField
      textFieldWithString:[NSString stringWithFormat:@"%d", self.workMin]];
  self.workField.frame = NSMakeRect(150, y, 100, 20);
  [cv addSubview:self.workField];
  y -= 30;

  // Break
  [[NSTextField labelWithString:@"Break (min):"]
      setFrame:NSMakeRect(20, y, 120, 20)];
  [cv addSubview:[cv.subviews lastObject]];
  self.breakField = [NSTextField
      textFieldWithString:[NSString stringWithFormat:@"%d", self.breakMin]];
  self.breakField.frame = NSMakeRect(150, y, 100, 20);
  [cv addSubview:self.breakField];
  y -= 30;

  // Long Break
  [[NSTextField labelWithString:@"Long Break (min):"]
      setFrame:NSMakeRect(20, y, 120, 20)];
  [cv addSubview:[cv.subviews lastObject]];
  self.longBreakField = [NSTextField
      textFieldWithString:[NSString stringWithFormat:@"%d", self.longBreakMin]];
  self.longBreakField.frame = NSMakeRect(150, y, 100, 20);
  [cv addSubview:self.longBreakField];
  y -= 30;

  // Sessions
  [[NSTextField labelWithString:@"Sessions until Long:"]
      setFrame:NSMakeRect(20, y, 120, 20)];
  [cv addSubview:[cv.subviews lastObject]];
  self.sessionsField = [NSTextField
      textFieldWithString:[NSString
                              stringWithFormat:@"%d", self.sessionsUntilLong]];
  self.sessionsField.frame = NSMakeRect(150, y, 100, 20);
  [cv addSubview:self.sessionsField];
  y -= 40;

  // Sound & AutoStart
  self.soundCheck = [NSButton checkboxWithTitle:@"Sound Enabled"
                                         target:nil
                                         action:nil];
  self.soundCheck.frame = NSMakeRect(20, y, 130, 20);
  self.soundCheck.state =
      self.soundOn ? NSControlStateValueOn : NSControlStateValueOff;
  [cv addSubview:self.soundCheck];

  self.autoStartCheck = [NSButton checkboxWithTitle:@"Auto-start"
                                             target:nil
                                             action:nil];
  self.autoStartCheck.frame = NSMakeRect(150, y, 130, 20);
  self.autoStartCheck.state =
      self.autoStart ? NSControlStateValueOn : NSControlStateValueOff;
  [cv addSubview:self.autoStartCheck];
  y -= 40;

  // Opacity
  [[NSTextField labelWithString:@"Opacity:"]
      setFrame:NSMakeRect(20, y, 60, 20)];
  [cv addSubview:[cv.subviews lastObject]];
  self.opacitySlider = [NSSlider sliderWithValue:self.opacity
                                        minValue:10
                                        maxValue:100
                                          target:nil
                                          action:nil];
  self.opacitySlider.frame = NSMakeRect(80, y, 180, 20);
  [cv addSubview:self.opacitySlider];
  y -= 30;

  // Volume
  [[NSTextField labelWithString:@"Volume:"] setFrame:NSMakeRect(20, y, 60, 20)];
  [cv addSubview:[cv.subviews lastObject]];
  self.volumeSlider = [NSSlider sliderWithValue:self.volume
                                       minValue:0
                                       maxValue:128
                                         target:nil
                                         action:nil];
  self.volumeSlider.frame = NSMakeRect(80, y, 180, 20);
  [cv addSubview:self.volumeSlider];
  y -= 30;

  // Focus Threshold
  [[NSTextField labelWithString:@"Focus Threshold (s):"]
      setFrame:NSMakeRect(20, y, 130, 20)];
  [cv addSubview:[cv.subviews lastObject]];
  self.focusField = [NSTextField
      textFieldWithString:[NSString
                              stringWithFormat:@"%d", self.focusThreshold]];
  self.focusField.frame = NSMakeRect(150, y, 100, 20);
  [cv addSubview:self.focusField];
  y -= 50;

  // Apply
  NSButton *applyBtn = [NSButton buttonWithTitle:@"Apply Settings"
                                          target:self
                                          action:@selector(applySettings)];
  applyBtn.frame = NSMakeRect(75, y, 150, 30);
  [cv addSubview:applyBtn];

  [self.settingsWindow makeKeyAndOrderFront:nil];
}

- (void)applySettings {
  int newW = [self.workField intValue];
  int newB = [self.breakField intValue];
  int newLB = [self.longBreakField intValue];
  int newS = [self.sessionsField intValue];
  BOOL newSound = (self.soundCheck.state == NSControlStateValueOn);
  BOOL newAuto = (self.autoStartCheck.state == NSControlStateValueOn);
  int newOp = [self.opacitySlider intValue];
  int newVol = [self.volumeSlider intValue];
  int newFT = [self.focusField intValue];

  self.workMin = newW;
  self.breakMin = newB;
  self.longBreakMin = newLB;
  self.sessionsUntilLong = newS;
  self.soundOn = newSound;
  self.autoStart = newAuto;
  self.opacity = newOp;
  self.volume = newVol;
  self.focusThreshold = newFT;

  self.pomoView.workMin = self.workMin;
  self.pomoView.breakMin = self.breakMin;
  [self.window setAlphaValue:self.opacity / 100.0];
  if (self.audioPlayer) {
    self.audioPlayer.volume = self.volume / 128.0;
    if (self.soundOn)
      [self.audioPlayer play];
    else
      [self.audioPlayer stop];
  }

  [self resetTimer];
  [self saveConfig];
  [self.settingsWindow close];
  self.settingsWindow = nil;
}

- (void)showStreak {
  if (self.streakWindow) {
    [self.streakWindow makeKeyAndOrderFront:nil];
    return;
  }

  NSRect frame = NSMakeRect(0, 0, 750, 200);
  self.streakWindow = [[NSWindow alloc]
      initWithContentRect:frame
                styleMask:NSWindowStyleMaskTitled | NSWindowStyleMaskClosable
                  backing:NSBackingStoreBuffered
                    defer:NO];
  [self.streakWindow setTitle:@"Streak Counter"];
  [self.streakWindow center];

  NSView *cv = [[NSView alloc] initWithFrame:frame];
  [self.streakWindow setContentView:cv];

  CGFloat y = 170;
  NSTextField *dailyLabel = [NSTextField
      labelWithString:[NSString stringWithFormat:@"Daily Sessions: %d",
                                                 self.dailySessions]];
  dailyLabel.frame = NSMakeRect(20, y, 150, 20);
  [cv addSubview:dailyLabel];

  NSTextField *consecutiveLabel = [NSTextField
      labelWithString:[NSString stringWithFormat:@"Consecutive Days: %d",
                                                 self.consecutiveDays]];
  consecutiveLabel.frame = NSMakeRect(200, y, 180, 20);
  [cv addSubview:consecutiveLabel];

  NSTextField *lastLabel = [NSTextField
      labelWithString:[NSString
                          stringWithFormat:@"Last Active: %@", self.lastDate]];
  lastLabel.frame = NSMakeRect(400, y, 200, 20);
  [cv addSubview:lastLabel];

  StreakGraphView *gv =
      [[StreakGraphView alloc] initWithFrame:NSMakeRect(0, 0, 750, 160)];
  gv.history = self.history;
  [cv addSubview:gv];

  [self.streakWindow makeKeyAndOrderFront:nil];

  // Listen for window close to nil out property
  [[NSNotificationCenter defaultCenter]
      addObserverForName:NSWindowWillCloseNotification
                  object:self.streakWindow
                   queue:[NSOperationQueue mainQueue]
              usingBlock:^(NSNotification *note) {
                self.streakWindow = nil;
              }];
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
