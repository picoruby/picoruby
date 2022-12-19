puts "RGB started"

sleep 0.5

while true
  unless $rgb
    sleep 10
  else
    $rgb.show
  end
end

