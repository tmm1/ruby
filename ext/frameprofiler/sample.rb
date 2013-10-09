require 'pp'
require 'frameprofiler'

class ProfileResult
  def initialize(data)
    @data = data
  end

  def frames
    @data[:frames].sort_by{ |iseq, stats| -stats[:samples] }
  end

  def overall_samples
    @data[:samples]
  end

  def print_debug
    pp @data
  end

  def print_graphviz
    f = STDOUT
    f.puts "digraph profile {"
    frames.each do |frame, info|
      call, total = info.values_at(:samples, :total_samples)
      sample = ''
      sample << "#{call} (%2.1f%%)\\rof " % (call*100.0/overall_samples) if call < total
      sample << "#{total} (%2.1f%%)\\r" % (total*100.0/overall_samples)
      size = (1.0 * call / overall_samples) * 28 + 10

      f.puts "  #{frame} [size=#{size}] [fontsize=#{size}] [shape=box] [label=\"#{info[:name]}\\n#{sample}\"];"
      if edges = info[:edges]
        edges.each do |edge, weight|
          size = (1.0 * weight / overall_samples) * 28
          f.puts "  #{frame} -> #{edge} [label=\"#{weight}\"];"
        end
      end
    end
    f.puts "}"
  end

  def print_text
    printf "% 10s    (pct)  % 10s    (pct)     FRAME\n" % ["TOTAL", "SAMPLES"]
    frames.each do |frame, info|
      call, total = info.values_at(:samples, :total_samples)
      printf "% 10d % 8s  % 10d % 8s     %s\n", total, "(%2.1f%%)" % (total*100.0/overall_samples), call, "(%2.1f%%)" % (call*100.0/overall_samples), info[:name]
    end
  end

  def print_method(name)
    name = /#{Regexp.escape name}/ unless Regexp === name
    frames.each do |frame, info|
      next unless info[:name] =~ name
      file, line = info.values_at(:file, :line)

      line = line.to_i - 1
      maxline = info[:lines] ? info[:lines].keys.max : line + 5
      printf "%s (%s:%d)\n", info[:name], file, line

      source = File.readlines(file).each_with_index do |code, i|
        next unless (line..maxline).include?(i)
        if samples = info[:lines][i+1]
          printf "% 5d % 7s / % 7s  | % 5d  | %s", samples, "(%2.1f%%" % (100.0*samples/overall_samples), "%2.1f%%)" % (100.0*samples/info[:samples]), i+1, code
        else
          printf "                         | % 5d  | %s", i+1, code
        end
      end
    end
  end
end

class A
  def initialize
    pow
    self.class.newobj
    math
  end

  def pow
    2 ** 100
  end

  def self.newobj
    Object.new
    Object.new
  end

  def math
    2.times do
      2 + 3 * 4 ^ 5 / 6
    end
  end
end

#profile = FrameProfiler.run(:object, 1) do
#profile = FrameProfiler.run(:wall, 1000) do
profile = FrameProfiler.run(:cpu, 1000) do
  1_000_000.times do
    A.new
  end
end

result = ProfileResult.new(profile)
puts
result.print_method(/pow|newobj/)
puts
result.print_text
puts
result.print_graphviz
puts
result.print_debug

__END__

tmm1@tmm1-air ~/code/ruby-gitsvn (trunk*) $ ./ruby -I .ext/x86_64-darwin12.4.1/:lib:. ext/frameprofiler/sample.rb

A#pow (/Users/tmm1/code/ruby-gitsvn/ext/frameprofiler/sample.rb:79)
                         |    79  |   def pow
  194  (44.7% / 100.0%)  |    80  |     2 ** 100
                         |    81  |   end
A.newobj (/Users/tmm1/code/ruby-gitsvn/ext/frameprofiler/sample.rb:83)
                         |    83  |   def self.newobj
   54  (12.4% /  50.9%)  |    84  |     Object.new
   52  (12.0% /  49.1%)  |    85  |     Object.new
                         |    86  |   end

     TOTAL    (pct)      CALLER    (pct)     FRAME
       361  (47.9%)         361  (47.9%)     A#pow
       203  (26.9%)         203  (26.9%)     A.newobj
       147  (19.5%)         147  (19.5%)     block in A#math
       754 (100.0%)          27   (3.6%)     block (2 levels) in <main>
       158  (21.0%)          11   (1.5%)     A#math
       727  (96.4%)           5   (0.7%)     A#initialize
       754 (100.0%)           0   (0.0%)     <main>
       754 (100.0%)           0   (0.0%)     block in <main>
       754 (100.0%)           0   (0.0%)     <main>

digraph profile {
  70261185608980 [size=23.405835543766578] [fontsize=23.405835543766578] [shape=box] [label="A#pow\n361 (47.9%)\r"];
  70261185608880 [size=17.53846153846154] [fontsize=17.53846153846154] [shape=box] [label="A.newobj\n203 (26.9%)\r"];
  70261185608680 [size=15.458885941644564] [fontsize=15.458885941644564] [shape=box] [label="block in A#math\n147 (19.5%)\r"];
  70261185608440 [size=11.0026525198939] [fontsize=11.0026525198939] [shape=box] [label="block (2 levels) in <main>\n27 (3.6%)\rof 754 (100.0%)\r"];
  70261185608440 -> 70261185609100 [label="727"];
  70261185608780 [size=10.408488063660478] [fontsize=10.408488063660478] [shape=box] [label="A#math\n11 (1.5%)\rof 158 (21.0%)\r"];
  70261185608780 -> 70261185608680 [label="147"];
  70261185609100 [size=10.185676392572944] [fontsize=10.185676392572944] [shape=box] [label="A#initialize\n5 (0.7%)\rof 727 (96.4%)\r"];
  70261185609100 -> 70261185608980 [label="361"];
  70261185609100 -> 70261185608780 [label="158"];
  70261185609100 -> 70261185608880 [label="203"];
  70261177451380 [size=10.0] [fontsize=10.0] [shape=box] [label="<main>\n0 (0.0%)\rof 754 (100.0%)\r"];
  70261177451380 -> 70261185586320 [label="754"];
  70261185608520 [size=10.0] [fontsize=10.0] [shape=box] [label="block in <main>\n0 (0.0%)\rof 754 (100.0%)\r"];
  70261185608520 -> 70261185608440 [label="754"];
  70261185586320 [size=10.0] [fontsize=10.0] [shape=box] [label="<main>\n0 (0.0%)\rof 754 (100.0%)\r"];
  70261185586320 -> 70261185608520 [label="754"];
}

{:version=>1.0,
 :mode=>"cpu(1000)",
 :samples=>754,
 :frames=>
  {70261185608980=>
    {:name=>"A#pow",
     :location=>"/Users/tmm1/code/ruby-gitsvn/ext/frameprofiler/sample.rb:117",
     :total_samples=>361,
     :samples=>361,
     :lines=>{59=>361}},
   70261185609100=>
    {:name=>"A#initialize",
     :location=>"/Users/tmm1/code/ruby-gitsvn/ext/frameprofiler/sample.rb:105",
     :total_samples=>727,
     :samples=>5,
     :edges=>{70261185608980=>361, 70261185608780=>158, 70261185608880=>203},
     :lines=>{55=>5}},
   70261185608440=>
    {:name=>"block (2 levels) in <main>",
     :location=>"/Users/tmm1/code/ruby-gitsvn/ext/frameprofiler/sample.rb:151",
     :total_samples=>754,
     :samples=>27,
     :edges=>{70261185609100=>727},
     :lines=>{76=>27}},
   70261185608520=>
    {:name=>"block in <main>",
     :location=>"/Users/tmm1/code/ruby-gitsvn/ext/frameprofiler/sample.rb:149",
     :total_samples=>754,
     :samples=>0,
     :edges=>{70261185608440=>754}},
   70261185586320=>
    {:name=>"<main>",
     :location=>"/Users/tmm1/code/ruby-gitsvn/ext/frameprofiler/sample.rb",
     :total_samples=>754,
     :samples=>0,
     :edges=>{70261185608520=>754}},
   70261177451380=>
    {:name=>"<main>",
     :location=>"ext/frameprofiler/sample.rb",
     :total_samples=>754,
     :samples=>0,
     :edges=>{70261185586320=>754}},
   70261185608680=>
    {:name=>"block in A#math",
     :location=>"/Users/tmm1/code/ruby-gitsvn/ext/frameprofiler/sample.rb:137",
     :total_samples=>147,
     :samples=>147,
     :lines=>{69=>147}},
   70261185608780=>
    {:name=>"A#math",
     :location=>"/Users/tmm1/code/ruby-gitsvn/ext/frameprofiler/sample.rb:135",
     :total_samples=>158,
     :samples=>11,
     :edges=>{70261185608680=>147},
     :lines=>{68=>11}},
   70261185608880=>
    {:name=>"A.newobj",
     :location=>"/Users/tmm1/code/ruby-gitsvn/ext/frameprofiler/sample.rb:125",
     :total_samples=>203,
     :samples=>203,
     :lines=>{63=>96, 64=>107}}}}
