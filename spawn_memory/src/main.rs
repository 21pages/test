use jemalloc_ctl::{epoch, stats};
use tokio::time::{sleep, Duration};

#[global_allocator]
static ALLOC: jemallocator::Jemalloc = jemallocator::Jemalloc;

#[tokio::main]
async fn main() {
    let mut last_allocated = 0;
    let mut last_resident = 0;
    let mut handles = vec![];
    for i in 0..10000 {
        handles.push(tokio::spawn(task()));
        if i % 100 == 0 {
            epoch::advance().unwrap();
            let allocated = stats::allocated::read().unwrap();
            let resident = stats::resident::read().unwrap();
            println!(
                "i:{}, {} bytes allocated/{} bytes resident",
                i,
                allocated - last_allocated,
                resident - last_resident
            );
            last_allocated = allocated;
            last_resident = resident;
        }
    }
    for h in handles {
        h.await.unwrap();
    }
}

async fn task() {
    sleep(Duration::from_millis(1000)).await;
}
