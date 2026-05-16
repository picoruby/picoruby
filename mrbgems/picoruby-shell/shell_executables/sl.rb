# sl.rb for PicoRuby / mruby-compiler2
# final tuned edition: closer ASCII placement to original sl.c
#
# options:
#   -a  accident
#   -l  long train
#   -F  flying
#   -e  leave screen without final clear
#   -h  help
#
# tuned for 80x24 terminal
# no Float / no rescue/ensure
# avoids constant redefinition warnings

opts = []

ARGV.each do |arg|
  if arg.start_with?("-")
    s = arg[1, 255]
    if s
      s.each_char { |ch| opts << ch }
    end
  end
end

if opts.include?("h")
  puts "usage: sl [-alFeh]"
  exit 0
end

opt_a = opts.include?("a")
opt_l = opts.include?("l")
opt_f = opts.include?("F")
opt_e = opts.include?("e")

w = (ENV["COLUMNS"] || "80").to_i
h = (ENV["LINES"] || "24").to_i
w = 80 if w <= 0
h = 24 if h <= 0
w = 80 if w > 80

def move_to(row, col)
  print "\e[#{row};#{col}H"
end

def draw_line(row, x, line, w, h)
  return if row < 1 || row > h
  return if x >= w
  return if (x + line.length) <= 0

  if x < 0
    cut = -x
    out = line[cut, line.length - cut]
    out = "" if out.nil?
    move_to(row, 1)
    print out[0, w]
  else
    move_to(row, 1)
    print ((" " * x) + line)[0, w]
  end
end

def sprite_width(pic)
  m = 0
  i = 0
  while i < pic.length
    s = pic[i]
    m = s.length if s.length > m
    i += 1
  end
  m
end

def concat_pics(left, right)
  out = []
  max = left.length
  max = right.length if right.length > max

  i = 0
  while i < max
    l = i < left.length ? left[i] : ""
    r = i < right.length ? right[i] : ""
    out << (l + r)
    i += 1
  end
  out
end

def repeat_pic(head, car, count)
  out = head
  i = 0
  while i < count
    out = concat_pics(out, car)
    i += 1
  end
  out
end

# =========================================================
# closer-to-original locomotive and coaches
# =========================================================

engine1 = [
  "      ====        ________                ___________ ",
  "  _D _|  |_______/        \\__I_I_____===__|_________| ",
  "   |(_)---  |   H\\________/ |   |        =|___ ___|   ",
  "   /     |  |   H  |  |     |   |         ||_| |_||   ",
  "  |      |  |   H  |__--------------------| [___] |   ",
  "  | ________|___H__/__|_____/[][]~\\_______|       |   ",
  "  |/ |   |-----------I_____I [][] []  D   |=======|__ ",
  "__/ =| o |=-O=====O=====O=====O \\ ____Y___________|__ ",
  " |/-=|___|=    ||    ||    ||   |_____/~\\___/        ",
  "  \\_/      \\__/  \\__/  \\__/  \\__/      \\_/           "
]

engine2 = [
  "      ====        ________                ___________ ",
  "  _D _|  |_______/        \\__I_I_____===__|_________| ",
  "   |(_)---  |   H\\________/ |   |        =|___ ___|   ",
  "   /     |  |   H  |  |     |   |         ||_| |_||   ",
  "  |      |  |   H  |__--------------------| [___] |   ",
  "  | ________|___H__/__|_____/[][]~\\_______|       |   ",
  "  |/ |   |-----------I_____I [][] []   D  |=======|__ ",
  "__/ =| o |=-~O=====O=====O=====O\\ ____Y___________|__ ",
  " |/-=|___|=   ||    ||    ||    |_____/~\\___/        ",
  "  \\_/      \\_/  \\__/  \\__/  \\___/      \\_/           "
]

car1 = [
  "                              _______________________ ",
  "                             _|                       |",
  "                            / |     ||   ||   ||      |",
  "                           /  |_______________________|",
  "                           |  | .-------------------. |",
  "                           |  | | []  []  []  []   | |",
  "                           |  | |                   | |",
  "                           |  | '-------------------' |",
  "                           |  |__|||__|||__|||__|||__| ",
  "                           |_/   O     O     O    \\_| "
]

car2 = [
  "                              _______________________ ",
  "                             _|                       |",
  "                            / |     ||   ||   ||      |",
  "                           /  |_______________________|",
  "                           |  | .-------------------. |",
  "                           |  | | []  []  []  []   | |",
  "                           |  | |                   | |",
  "                           |  | '-------------------' |",
  "                           |  |__|O|__|||__|O|__|||__| ",
  "                           |_/    \\_/     \\_/    \\_|  "
]

# =========================================================
# accident mode people
# =========================================================

man1 = [
  "   O  ",
  "  /|\\ ",
  "  / \\ "
]

man2 = [
  " \\O   ",
  "  |\\  ",
  " / \\  "
]

man3 = [
  "  O/  ",
  " /|   ",
  " / \\  "
]

man4 = [
  " _O_  ",
  "  |   ",
  " / \\  "
]

# =========================================================
# smoke: tuned to look closer to original balance
# =========================================================

smoke_patterns = [
  ["      (   ) ", "     (    ) ", "      (   ) "],
  ["       ( )  ", "      (   ) ", "       ( )  "],
  ["        ()  ", "       ( )  ", "        ()  "],
  ["        O   ", "       O O  ", "        O   "],
  ["        o   ", "       o o  ", "        o   "],
  ["        .   ", "       . .  ", "        .   "]
]

def compose_train(frame, long_mode, engine1, engine2, car1, car2)
  eng = ((frame & 1) == 0) ? engine1 : engine2
  return eng unless long_mode

  car = ((frame & 1) == 0) ? car1 : car2
  repeat_pic(eng, car, 3)
end

train_h = engine1.length
normal_w = sprite_width(engine1)
long_w = sprite_width(repeat_pic(engine1, car1, 3))
train_w = opt_l ? long_w : normal_w

base_y = (h - train_h) / 2
base_y = 2 if base_y < 2

fly_top = 2
fly_mid = 4
fly_bottom = base_y
fly_span = w + train_w

frame = 0
x = w
smokes = []

print "\e[?25l"

while x > -train_w
  print "\e[2J\e[H"

  pic = compose_train(frame, opt_l, engine1, engine2, car1, car2)

  current_y = base_y
  if opt_f
    progress = w - x
    progress = 0 if progress < 0
    progress = fly_span if progress > fly_span
    q = progress * 100 / fly_span

    if q < 15
      current_y = fly_top
    elsif q < 35
      current_y = fly_top + 1
    elsif q < 55
      current_y = fly_mid
    elsif q < 75
      current_y = fly_mid + 1
    else
      current_y = fly_bottom
    end
  end

  current_y = 1 if current_y < 1

  i = 0
  while i < pic.length
    draw_line(current_y + i, x, pic[i], w, h)
    i += 1
  end

  smoke_x = x + 6
  smoke_y = current_y - 1
  smoke_y = 1 if smoke_y < 1

  if (frame % 2) == 0
    smokes << [smoke_x, smoke_y, 0]
  end

  if opt_a
    people = []
    people << [x + 13, current_y + train_h - 3, ((frame & 3) == 0 ? man1 : man2)]
    people << [x + 20, current_y + train_h - 3, ((frame & 3) == 1 ? man3 : man4)]
    people << [x + 27, current_y + train_h - 3, ((frame & 3) == 2 ? man2 : man1)]
    people << [x + 34, current_y + train_h - 3, ((frame & 3) == 3 ? man4 : man3)]

    if opt_l
      people << [x + 60,  current_y + train_h - 3, man2]
      people << [x + 84,  current_y + train_h - 3, man1]
      people << [x + 108, current_y + train_h - 3, man3]
    end

    pi = 0
    while pi < people.length
      px = people[pi][0]
      py = people[pi][1]
      pp = people[pi][2]

      pj = 0
      while pj < pp.length
        draw_line(py + pj, px, pp[pj], w, h)
        pj += 1
      end
      pi += 1
    end
  end

  new_smokes = []
  i = 0
  while i < smokes.length
    sx = smokes[i][0]
    sy = smokes[i][1]
    age = smokes[i][2]

    if age < smoke_patterns.length
      pat = smoke_patterns[age]

      j = 0
      while j < pat.length
        draw_line(sy + j, sx, pat[j], w, h)
        j += 1
      end

      if opt_f
        nx = sx - 2 - (age / 2)
        ny = sy - 1
      else
        nx = sx - 1 - (age / 2)
        ny = sy - 1
      end

      nage = age + 1
      new_smokes << [nx, ny, nage] if ny > 0
    end

    i += 1
  end
  smokes = new_smokes

  frame += 1
  x -= 1
  sleep_ms(50)
end

print "\e[0m"
print "\e[?25h"

if opt_e
  move_to(h, 1)
  puts
else
  print "\e[2J\e[H"
end