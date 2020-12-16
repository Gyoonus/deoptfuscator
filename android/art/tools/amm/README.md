# Actionable Memory Metric

The goal of the actionable memory metric (AMM) is to provide a view of an
application's memory use that application developers can track, understand,
and control. AMM can be thought of as a Java heap dump augmented with models
for non-Java allocations that app developers have some control of.

There are two components of the actionable memory metric:
1. The value of the metric.
2. An actionable breakdown of the value of the metric.

The metric is made up of a collection of separate models for different
categories of memory use. Each model contributes to the value and actionable
breakdown of the overall metric.

See models/ for a list of models proposed for the actionable memory metric.
