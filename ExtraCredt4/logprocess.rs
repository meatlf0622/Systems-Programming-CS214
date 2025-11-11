use std::env;
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::sync::Arc;
use std::thread;
use std::time::Instant;

fn main() {
    
    let args: Vec<String> = env::args().collect();
    if args.len() != 4 {
        eprintln!("Usage: {} <filename> <searchTerm> <numThreads>", args[0]);
        return;
    }

    let fileName = &args[1];
    let searchTerm = &args[2];
    let numThreads: usize = args[3].parse().expect("Invalid number of threads");
    let startTime = Instant::now();

    let file = File::open(fileName).expect("Failed to open file");
    let reader = BufReader::new(file);
    let lines: Vec<String> = reader.lines().map(|line| line.unwrap()).collect();

    let totalLines = lines.len();
    let chunkSize = (totalLines + numThreads - 1) / numThreads;
    let sharedLines = Arc::new(lines);
    let mut handles = Vec::new();

    for i in 0..numThreads {
        let linesClone = Arc::clone(&sharedLines);
        let search = searchTerm.clone();
        let start = i * chunkSize;
        let end = std::cmp::min(start + chunkSize, totalLines);

        let handle = thread::spawn(move || {
            let mut matches = Vec::new();
            for line in &linesClone[start..end] {
                if line.contains(&search) {
                    matches.push(line.clone());
                }
            }
            (start, matches)
        });

        handles.push(handle);
    }

    let mut allMatches = Vec::new();
    for handle in handles {
        let (startIndex, matchLines) = handle.join().unwrap();
        allMatches.push((startIndex, matchLines));
    }

    allMatches.sort_by_key(|(startIndex, _)| *startIndex);
    for (_, matchLines) in allMatches {
        for line in matchLines {
            println!("{}", line);
        }
    }

    let elapsed = startTime.elapsed().as_nanos();
    println!("{} processed in {} nanoseconds", fileName, elapsed);
}
