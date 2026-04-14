#!/bin/bash
for file in /home/xixi/code/fbb_ws63_20260114/src/application/mine/ws63_final/App/Task/*.c; do
    # Extract the name of the lock function
    LOCK_DEF=$(grep -E 'static unsigned int ws63_.*_lock\(' "$file" | sed -E 's/static unsigned int (ws63_.*_lock)\(void\)/\1/')
    UNLOCK_DEF=$(grep -E 'static void ws63_.*_unlock\(' "$file" | sed -E 's/static void (ws63_.*_unlock)\(unsigned int.*\)/\1/')
    
    if [ ! -z "$LOCK_DEF" ] && [ ! -z "$UNLOCK_DEF" ]; then
        echo "Processing $file"
        echo "Found lock: $LOCK_DEF, unlock: $UNLOCK_DEF"
        
        # Remove the function definitions. We assume they are 4 lines each.
        sed -i "/static unsigned int $LOCK_DEF(void)/,+3d" "$file"
        sed -i "/static void $UNLOCK_DEF(unsigned int irq_status)/,+3d" "$file"
        
        # Replace occurrences.
        sed -i "s/$LOCK_DEF()/WS63_FINAL_IRQ_LOCK()/g" "$file"
        sed -i "s/$UNLOCK_DEF/WS63_FINAL_IRQ_UNLOCK/g" "$file"
    fi
done
