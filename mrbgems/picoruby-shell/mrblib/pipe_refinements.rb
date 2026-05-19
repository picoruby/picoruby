if RUBY_ENGINE == "mruby/c"
  # FemtoRuby (mruby/c) has no Module#refine / task-scoped refinements,
  # so the pipeline plumbing that depends on per-task method override is
  # not available. Define the constants as nil so other files can detect
  # the missing capability without raising NameError on first reference.
  PipeOUT = nil
  PipeIN  = nil
  PIPELINE_USING = nil
else
  module PipeOUT
    refine Kernel do
      def puts(*args)
        PipelineIO.puts(Task.current.name, *args)
      end

      def print(*args)
        PipelineIO.print(Task.current.name, *args)
      end
    end
  end

  module PipeIN
    refine Kernel do
      def gets
        PipelineIO.gets(Task.current.name)
      end

      def getc
        PipelineIO.getc(Task.current.name)
      end
    end
  end

  PIPELINE_USING = [PipeOUT, PipeIN]
end
