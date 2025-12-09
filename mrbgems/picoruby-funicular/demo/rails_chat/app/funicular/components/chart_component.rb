class ChartComponent < Funicular::Component
  def initialize(props = {})
    super(props)
    @channel_id = props[:channel_id]
  end

  def initialize_state
    {
      stats: []
    }
  end

  def component_mounted
    return unless @channel_id

    # Subscribe to StatsChannel
    consumer = Funicular::Cable.create_consumer("/cable")
    @stats_subscription = consumer.subscriptions.create(
      { channel: "StatsChannel", channel_id: @channel_id }
    ) do |data|
      case data["type"]
      when "stats_update"
        patch(stats: data["stats"])
        update_chart if @chart
      end
    end

    # Initialize Chart.js
    canvas = @refs[:chart_canvas]
    return unless canvas

    config = {
      type: 'bar',
      data: {
        labels: [],
        datasets: [{
          label: 'Messages (24h)',
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
  end

  def component_will_unmount
    @stats_subscription.unsubscribe if @stats_subscription
    @chart.destroy if @chart
  end

  def update_chart
    return unless @chart

    labels = state.stats.map { |s| s["username"] }
    data = state.stats.map { |s| s["count"] }

    @chart[:data][:labels] = JS::Bridge.to_js(labels)
    @chart[:data][:datasets][0][:data] = JS::Bridge.to_js(data)
    @chart.update("none")
  end

  def render
    div(class: "w-80 bg-white border-l border-gray-200 p-4 flex flex-col") do
      h3(class: "text-lg font-bold text-gray-800 mb-4") { "Activity (24h)" }

      div(class: "flex-1 flex items-center justify-center") do
        if state.stats.empty?
          div(class: "text-center text-gray-500") do
            span { "No messages yet" }
          end
        else
          div(class: "w-full h-64") do
            canvas(ref: :chart_canvas)
          end
        end
      end

      div(class: "mt-4 text-xs text-gray-500 text-center") do
        span { "Messages per user in the last 24 hours" }
      end
    end
  end
end
