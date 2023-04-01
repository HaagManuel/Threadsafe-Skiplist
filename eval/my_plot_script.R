# Loads libraries
library(tidyverse)
library(dplyr)
library(scales)
library(ggpubr)
library(ggtext)

#note: wd = /eval

# Path to the result files
# res_folder='./exercise2/build/'
res_folder='../build/'
out_folder='./'

header = c("it", "threads", "n", "p", "max_level", "sorting", "benchmark", "variant", "time") 
header2 = c("it", "threads", "n", "p", "max_level", "sorting", "benchmark", "variant", "time",
            "num_find", "num_find_retry", "total_op") 

header3 = c("it", "sec", "threads", "n", "p", "max_level", "sorting", "benchmark", "variant", "time") 


add_group_label1 = function(df) {
  df$Group = paste(df$sorting, df$benchmark, df$variant)
  return(df)
}

add_group_label2 = function(df) {
  df$Group = paste(df$variant, df$benchmark, df$sorting)
  return(df)
}

add_group_label3 = function(df) {
  # df$group = paste(df$sorting, df$benchmark, df$variant)
  df$Group = paste(df$sorting, df$variant)
  return(df)
}

vary_p = read.table("./vary_p.txt", comment.char = '#', col.names = header)
vary_p = add_group_label1(vary_p)

scaling = read.table("./scaling.txt", comment.char = '#', col.names = header)
scaling = add_group_label1(scaling)

vary_n = read.table("./vary_n.txt", comment.char = '#', col.names = header)
vary_n = add_group_label1(vary_n)

counter = read.table("./counter.txt", comment.char = '#', col.names = header2)
counter = add_group_label2(counter)

rank = read.table("./rank.txt", comment.char = '#', col.names = header3)
rank = add_group_label3(rank)
# View(rank)

n1 = 1e+05
n2 = 1e+06





############## Plot Time ####################


############## Plot by p ####################
plot_time_p <- function(data) {
  ggplot(data, aes(x = p, y = time, group = Group, color = Group)) +
    stat_summary(fun = mean, geom = 'line') +
    scale_x_continuous(breaks=(0:10) / 10) +
    scale_y_continuous(trans="log10") +
    ggtitle("Average runtime by probabilty p") +
    xlab("p") +
    ylab("Avg Time [ms]")
}

plot_time_p(vary_p)

############## Plot by p ####################


############## Absolut Speedup Plot ####################
# n = 1e+05, 1e+06
plot_abs_speedup <- function(data, n1) {
  data = scaling %>% 
    filter(n == n1) %>% 
    group_by(threads, sorting, benchmark, variant, Group) %>%
    summarise(avg_t = mean(time)) 
  data$seq_time = 1
  for(x in unique(data$sorting)) {
    for(y in unique(data$benchmark))
      data[data$sorting==x & data$benchmark==y,]$seq_time = data[data$sorting==x & data$benchmark==y & data$variant=="sequential",]$avg_t
  }
  data$abs_speedup = data$seq_time / data$avg_t
  data = subset(data, variant != "sequential")
  ggplot(data, aes(x = threads, y = abs_speedup, group = Group, color = Group)) +
    geom_line() +
    scale_x_continuous(breaks=1:max(data$threads)) +
    scale_y_continuous(breaks=c(0.25, 0.5, 1:ceiling(max(data$abs_speedup)))) +
    geom_hline(yintercept=1) +
    ggtitle(paste("Absolut speedups,", " n = ", n1, sep = "")) +
    xlab("Threads") +
    ylab("Absolut Speedup: T_seq / T_p") 
}

plot_abs_speedup(scaling, n1)
plot_abs_speedup(scaling, n2)
############## Absolut Speedup Plot ####################



############## Plot Time  ####################
plot_time <- function(data, n1) {
  # This plots the mean and standard error for each section, and connects the points with lines
  num_variants = length(unique(data$Group))
  data = subset(data, n == n1)
  ggplot(data, aes(x = threads, y = time, group = Group, color = Group, shape = Group)) +
    stat_summary(fun.data = mean_se, geom = 'pointrange') +
    stat_summary(fun = mean, geom = 'line') +
    scale_x_continuous(breaks=1:max(data$threads)) +
    scale_y_continuous(trans="log10") +
    scale_shape_manual(values=1:num_variants) +
    ggtitle(paste("Average runtime by threads,", " n = ", n1, sep = "")) +
    xlab("Threads") +
    ylab("Avg Time [ms]")
}

plot_time(subset(scaling, variant != "sequential"), n1)
plot_time(subset(scaling, variant != "sequential"), n2)
############## Plot Time  ####################

############## Plot time n ####################
plot_time_n <- function(data, t) {
  # This plots the mean and standard error for each section, and connects the points with lines
  num_variants = length(unique(data$Group))
  data = subset(data, threads == t)
  ns = 2^(1:22)
  labels = paste("2^", 1:22, sep="")
  ggplot(data, aes(x = n, y = time, group = Group, color = Group, shape = Group)) +
    stat_summary(fun.data = mean_se, geom = 'pointrange') +
    stat_summary(fun = mean, geom = 'line') +
    scale_x_continuous(trans="log2", breaks=ns, labels = labels) +
    scale_y_continuous(trans="log10") +
    scale_shape_manual(values=1:num_variants) +
    ggtitle(paste("Average runtime by n, threads = ", t, sep="")) +
    xlab("n") +
    ylab("Avg Time [ms]")
}
plot_time_n(vary_n, 6)
plot_time_n(vary_n, 12)

############## Plot time n ####################

############## Plot Counter  ####################
#x = group, y = finds or retries, fill = type of value
plot_barplot <- function(data) {
  data$finds = (data$num_find - data$total_op)/data$total_op
  data = data %>% 
    group_by(Group, threads) %>% 
    summarise(avg_t = mean(finds), sd_t = sd(finds))
  ggplot(data) + 
    geom_bar(aes(x = as.factor(threads), y = avg_t, fill = as.factor(Group)), position = position_dodge(0.9), stat = "identity") +
    geom_errorbar(aes(x=as.factor(threads), ymin=avg_t-sd_t, ymax=avg_t+sd_t, fill = as.factor(Group)), colour="black", position = position_dodge(0.9), size=1.5, width = 0.4) +
    labs(title = "Fraction of repeated finds for 6 and 12 threads, n = 10e6", x = "Threads", y = "(finds - op) / op", fill ="Group")
  
}
plot_barplot(counter)

############## Plot Counter  ####################



############## Text Disclaimer  ####################

plot_text = function(text, size) {
  ggplot() + 
    ggtitle(text) +
    theme(plot.title = element_text(size=size))
}

text2 = 
"  
Experiment Info:

machine: 6 core laptop with 12 threads

Shuffling:
  - permuation: std::shuffle (n random swaps)
  - weak-shuffle: n random swaps with adjacent elements in the array

Benchmark: 5 iterations, 20% insert, 20% remove, 60% search, Key = Value = int
  - Disjoint: the shuffled permutation is equally divided to each thread
  - Shared:: the shuffled permuation is given to each thread

"

plot_text(text2, 16)


text3 = 
"  
Experiment Info:

machine: 6 core laptop with 12 threads

Shuffling:
  - permuation: std::shuffle (n random swaps)
  - weak-shuffle: n random swaps with adjacent elements in the array

Benchmark: 5 iterations, k inserts, followed by k rank queries for each section (k = n / sections)
  - Disjoint: the current section is shuffled and equally divided to each thread
  
Variants:
  - Indexable_slist: k inserts (parallel), compute indizes (sequential), k rank queries (parallel)
  - vector_seq: push_back (sequential), std::sort (sequential), std::lowerbound to determine rank (parallel)
  - vector_par: push_back (sequential), __gnu_parallel::sort (parallel), std::lowerbound to determine rank (parallel)
"

############## Text Disclaimer  ####################



############## Plot Time  ####################
plot_time_sec <- function(data, s) {
  # This plots the mean and standard error for each section, and connects the points with lines
  num_variants = length(unique(data$Group))
  data = subset(data, sec == s)
  ggplot(data, aes(x = threads, y = time, group = Group, color = Group, shape = Group)) +
    stat_summary(fun.data = mean_se, geom = 'pointrange') +
    stat_summary(fun = mean, geom = 'line') +
    scale_x_continuous(breaks=1:max(data$threads)) +
    scale_y_continuous(trans="log10") +
    scale_shape_manual(values=1:num_variants) +
    ggtitle(paste("Average runtime by threads,", " n = 1e05, sections = ", s, sep = "")) +
    xlab("Threads") +
    ylab("Avg Time [ms]")
}




############## Plot Time  ####################

#x = group, y = avg_t, fill = sections

plot_barplot2 <- function(data, t) {
  # lab_x = c("per_index", "per_vec_seq", "")
  data = subset(data, threads == t)
  data = data %>% 
    group_by(Group, sec) %>% 
    summarise(avg_t = mean(time), sd_t = sd(time))
  ggplot(data) + 
    geom_bar(aes(x = as.factor(sec), y = avg_t, fill = as.factor(Group)), position = position_dodge(0.9), stat = "identity") +
    geom_errorbar(aes(x=as.factor(sec), ymin=avg_t-sd_t, ymax=avg_t+sd_t, fill = as.factor(Group)), colour="black", position = position_dodge(0.9), size=1.5, width = 0.4) +
    labs(title = paste("Average running time, n = 1e+05, threads = ", t, sep = "") , x = "Sections", y = "Avg Time [ms]", fill ="Group") +
    scale_y_log10() #+
  # scale_x_discrete(labels = 1:6)
  
}



############## PDF  ####################
# pdf(paste(out_folder, "plots.pdf", sep = "/"), width=12, height=6)
plot_text(text2, 16)
plot_time_p(vary_p)
plot_abs_speedup(scaling, n1)
plot_abs_speedup(scaling, n2)
plot_time(subset(scaling, variant != "sequential"), n1)
plot_time(subset(scaling, variant != "sequential"), n2)
plot_time_n(vary_n, 6)
plot_time_n(vary_n, 12)
plot_barplot(counter)
# dev.off()


rank1 = subset(rank, sorting == "permutation")
rank2 = subset(rank, sorting == "weak_shuffle")

# pdf(paste(out_folder, "plots2.pdf", sep = "/"), width=12, height=6)
plot_text(text3, 16)
plot_time_sec(rank1, 1)
plot_time_sec(rank1, 10)
plot_time_sec(rank1, 100)
plot_time_sec(rank1, 1000)

plot_time_sec(rank2, 1)
plot_time_sec(rank2, 10)
plot_time_sec(rank2, 100)
plot_time_sec(rank2, 1000)

plot_barplot2(rank, 1)
plot_barplot2(rank, 6)
# dev.off()

############## PDF  ####################


