package main

import (
	"bufio"
	"flag"
	"fmt"

	"io"
	"os"

	"strings"

	"Developer/resources/go/powerful_command_line_applications/interacting/todo"
)

var todoFileName = ".todo.json"

func main() {
  flag.Usage = func() {
    fmt.Fprintf(flag.CommandLine.Output(), "%s tool. Developed for The Pragmatic Bookshelf\n", os.Args[0])
    fmt.Fprintf(flag.CommandLine.Output(), "Copyright 2020\n")
    fmt.Fprintf(flag.CommandLine.Output(), "Usage information:")
    flag.PrintDefaults()
  }
  
  add := flag.Bool("add", false, "Add task to the ToDo list")
  list := flag.Bool("list", false, "List all tasks")
  verbose := flag.Bool("verbose", false, "List all tasks with information")
  open := flag.Bool("open", false, "List open tasks")
  complete := flag.Int("complete", 0, "Item to be completed")
  del := flag.Int("del", 0, "Item to be deleted")

  flag.Parse()

  if os.Getenv("TODO_FILENAME") != "" {
    todoFileName = os.Getenv("TODO_FILENAME")
  }

  l := &todo.List{}

  if err := l.Get(todoFileName); err != nil {
    fmt.Fprintln(os.Stderr, err)
    os.Exit(1)
  }
  
  switch {
  case *list:
    fmt.Print(l)
  case *complete > 0:
    if err := l.Complete(*complete); err != nil {
      fmt.Fprintln(os.Stderr, err)
      os.Exit(1)
    }

    if err := l.Save(todoFileName); err != nil {
      fmt.Fprintln(os.Stderr, err)
      os.Exit(1)
    }
  case *add:
    t, err := getTask(os.Stdin, flag.Args()...)
    if err != nil {
      fmt.Fprintln(os.Stderr, err)
      os.Exit(1)
    }

    l.Add(t)

    if err := l.Save(todoFileName); err != nil {
      fmt.Fprintln(os.Stderr, err)
      os.Exit(1)
    }
  case *del > 0:
    if err := l.Delete(*del); err != nil {
      fmt.Fprintln(os.Stderr, err)
      os.Exit(1)
    }

    if err := l.Save(todoFileName); err != nil {
      fmt.Fprintln(os.Stderr, err)
      os.Exit(1)
    }
  case *verbose:
    fmt.Print(l.Verbose())

  case *open:
    fmt.Print(l.Open())
  default:
    fmt.Fprintln(os.Stderr, "Invalid option")
    os.Exit(1)
  }
}

// getTask function decides where to get the description for a new task
// task from: arguments or STDIN
func getTask(r io.Reader, args ...string) (string, error) {
  if len(args) > 0 {
    return strings.Join(args, " "), nil 
  }

  s := bufio.NewScanner(r)
  s.Scan()

  if err := s.Err(); err != nil {
    return "", nil
  }

  if len(s.Text()) == 0 {
    return "", fmt.Errorf("Task cannot be blank")
  }

  return s.Text(), nil
}
