class StatsChartComponent < Funicular::Component
  styles do
    container "w-80 bg-white border-l border-gray-200 p-4 flex flex-col"
    title "text-lg font-bold text-gray-800 mb-4"
    chart_wrapper "flex-1 flex items-center justify-center"
    empty_message "text-center text-gray-500"
    chart_container "w-full h-64"
    footer "mt-4 text-xs text-gray-500 text-center"
  end

  def component_mounted
    initialize_chart_if_needed
  end

  def component_updated
    update_chart if @chart && props[:stats] && !props[:stats].empty?
  end

  def component_will_unmount
    @chart.destroy if @chart
  end

  def initialize_chart_if_needed
    return if @chart
    return unless @refs[:chart_canvas]
    return if props[:stats].nil? || props[:stats].empty?

    canvas = @refs[:chart_canvas]

    config = {
      type: 'bar',
      data: {
        labels: [],
        datasets: [{
          label: 'Messages (1 year)',
          data: [],
          backgroundColor: 'rgba(59, 130, 246, 0.5)',
          borderColor: 'rgb(59, 130, 246)',
          borderWidth: 1
        }]
      },
      options: {
        responsive: true,
        maintainAspectRatio: false,
        scales: {
          y: {
            beginAtZero: true,
            ticks: {
              stepSize: 1
            }
          }
        },
        plugins: {
          legend: {
            display: false
          }
        }
      }
    }

    js_config = JS::Bridge.to_js(config)
    @chart = JS.global[:Chart].new(canvas, js_config)
    update_chart
  end

  def update_chart
    return unless @chart
    return unless props[:stats]

    labels = props[:stats].map { |s| s["username"] }
    data = props[:stats].map { |s| s["count"] }

    @chart[:data][:labels] = JS::Bridge.to_js(labels)
    @chart[:data][:datasets][0][:data] = JS::Bridge.to_js(data)
    @chart.update("none")
  end

  def render
    div(class: s.container) do
      h3(class: s.title) { "Activity (1 year)" }

      div(class: s.chart_wrapper) do
        if props[:stats].nil? || props[:stats].empty?
          div(class: s.empty_message) do
            span { "No messages yet" }
          end
        else
          div(class: s.chart_container) do
            canvas(ref: :chart_canvas)
          end
        end
      end

      div(class: s.footer) do
        span { "Messages per user in the last year" }
      end
    end
  end
end
