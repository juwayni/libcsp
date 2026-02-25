package main

import (
	"fmt"
	"time"
)

func benchGoroutineCreation(n int) {
	start := time.Now()
	done := make(chan bool)
	for i := 0; i < n; i++ {
		go func() {
			done <- true
		}()
	}
	for i := 0; i < n; i++ {
		<-done
	}
	elapsed := time.Since(start)
	fmt.Printf("Goroutine creation (%d): %v (%v per op)\n", n, elapsed, elapsed/time.Duration(n))
}

func benchChannelPingPong(n int) {
	start := time.Now()
	c1 := make(chan bool)
	c2 := make(chan bool)
	go func() {
		for i := 0; i < n; i++ {
			c1 <- true
			<-c2
		}
	}()
	for i := 0; i < n; i++ {
		<-c1
		c2 <- true
	}
	elapsed := time.Since(start)
	fmt.Printf("Channel Ping-Pong (%d): %v (%v per op)\n", n, elapsed, elapsed/time.Duration(n))
}

func main() {
	fmt.Println("--- Go Benchmarks ---")
	benchGoroutineCreation(10000)
	benchChannelPingPong(10000)
}
