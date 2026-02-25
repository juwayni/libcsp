package main

import (
	"context"
	"fmt"
	"sync"
	"time"
)

func worker(ctx context.Context, id int, jobs <-chan int, results chan<- int, wg *sync.WaitGroup) {
	defer wg.Done()
	for {
		select {
		case <-ctx.Done():
			fmt.Printf("Worker %d: cancelled and exiting\n", id)
			return
		case job, ok := <-jobs:
			if !ok {
				fmt.Printf("Worker %d: no more jobs, exiting\n", id)
				return
			}
			fmt.Printf("Worker %d: processing job %d\n", id, job)
			time.Sleep(100 * time.Millisecond) // Simulate work
			results <- job * 2
		}
	}
}

func main() {
	ctx, cancel := context.WithCancel(context.Background())
	var wg sync.WaitGroup

	jobs := make(chan int, 10)
	results := make(chan int, 10)

	// Start 3 workers
	for i := 1; i <= 3; i++ {
		wg.Add(1)
		go worker(ctx, i, jobs, results, &wg)
	}

	// Send 5 jobs
	for i := 1; i <= 5; i++ {
		jobs <- i
	}
	close(jobs)

	// Collect results with a timeout
	go func() {
		time.Sleep(2 * time.Second)
		fmt.Println("Main: timeout reached, cancelling context")
		cancel()
	}()

	go func() {
		wg.Wait()
		close(results)
	}()

	for res := range results {
		fmt.Printf("Main: got result %d\n", res)
	}

	fmt.Println("Main: execution finished")
}
