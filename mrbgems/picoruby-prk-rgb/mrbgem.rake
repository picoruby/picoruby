MRuby::Gem::Specification.new('picoruby-prk-rgb') do |spec|
  spec.license = 'MIT'
  spec.author  = 'HASUMI Hitoshi'
  spec.summary = 'PRK Firmware / class RGB'

  spec.add_dependency 'picoruby-float-ext'

  spec.require_name = 'rgb'

  task_dir = "#{build_dir}/task"
  rgb_task = "#{build_dir}/task/rgb_task.c"

  file rgb_task => [task_dir, "#{dir}/task/rgb_task.rb"] do |f|
    sh "#{build.mrbcfile} -B rgb_task -o #{f.name} #{dir}/task/rgb_task.rb"
  end

  spec.objs << rgb_task

  directory task_dir
end

